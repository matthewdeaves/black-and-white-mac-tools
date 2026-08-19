# Black & White Mac Tools

<img src="icons/patcher-256.png" width="150" align="right" alt="">

Two small tools for **Black & White** and **Creature Isle** on old Macs.

**Black & White Patcher** makes the games run on **Mac OS X 10.5 Leopard**,
where they otherwise crash a few seconds after launch. They work on 10.4 Tiger;
on 10.5 they die during the opening logo movies with `EXC_BAD_ACCESS (SIGBUS)`
at address `0x6898`. This writes 48 bytes into the game's executable to fix it,
and can put them back.

**Black & White Display** sets the game's resolution, colour depth and
fullscreen mode, including resolutions the game's own setup screen does not
offer.

Neither ships any game content. They work on a copy of the game you already own.

**Status:** the patcher works and is tested on hardware, as a command line tool.
The two drag-and-drop apps and the display tool are being built.

## The bug in one paragraph

While the intro movies play, the game pumps the Mac event loop. A mouse-down
event reaches `SetMouseModFlags`, which loads a pointer to the key-binding
table, adds a member offset of `0x6798`, and calls
`BindableAction::GetPrimaryKey()` on the result. At that point in startup the
table pointer is still **null**, so `this` becomes `0 + 0x6798`, and the first
instruction of `GetPrimaryKey` reads `0x6798 + 0x100 = 0x6898`.

On Tiger the CFM runtime happens to map a readable 8 KB shared-memory region at
`0x6000`, so that bad read quietly returns junk and the game carries on. On
Leopard nothing readable is there, and the same null dereference is fatal.

So this is a bug in the game, not a difference between the two systems. The fix
is correct on both. Full write-up with the disassembly:
[docs/black-and-white.md](docs/black-and-white.md).

## What the patch does

It puts a guard on the front of `GetPrimaryKey`. If `this` is below `0x10000`,
which is what a null base plus a member offset looks like, it returns an
all-zero key instead of dereferencing. A real pointer runs the original code,
two instructions later.

The guard lives in the dead PowerPC traceback table of the preceding function.
That is debug metadata nothing reads at runtime, which the games' own crash logs
demonstrate: every application frame in them symbolises as `???`.

## Finding the patch site

The tool does not use a table of hardcoded offsets. It locates the site
structurally in whatever binary you point it at:

1. Parse the PEF container and find the uncompressed code section.
2. Find exactly one traceback symbol `.GetPrimaryKey__14BindableActionCFv`.
3. Walk back to its traceback table, which must start with a zero word preceded
   by that function's `blr`.
4. Walk back further to the function's first four instructions, which must be
   exactly `lwz r5,0x100(r4)` / `li r0,16` / `mr r7,r3` / `addi r6,r4,0x100`.
5. Take the preceding function's traceback table as the cave. It must be at
   least 56 bytes.

If any check fails it refuses to write. That is what makes it safe on builds
nobody has tested, and it is why the same code handles both games.

## Verified binaries

All four were located correctly and patched, and the two marked "on hardware"
were played on a Power Mac G5 Quad under 10.5.8.

| game | md5 before | md5 after | tested |
|---|---|---|---|
| Black & White 1.1.9 (Platinum DVD) | `cfdaf24d863e477ba2d0a47e16b552f5` | `afe1454b0801cb5c96a4130dc41f922a` | discovery only |
| Black & White 1.1.9 (no-CD) | `cb76781c9bdb2cdd85aa6d9b00024c18` | `204c83ea1c49ae8cbde16f6d2abe2ff6` | on hardware |
| Creature Isle 1.1.9 (Platinum DVD) | `bf037971f339f5297d952c5b28f3e75f` | `1c08838b6c2d956b691878ea2dd98c8f` | discovery only |
| Creature Isle 1.1.9 (no-CD) | `6990d3a0ee9bfb254ad2ce5b03f219cd` | `3390bf7aca29c0906dec2380496a2f4c` | on hardware |

## Use

Quit the game first. The executable is mapped while it runs.

```
oldmacpatch scan   "/Applications/Black & White"     # report, change nothing
oldmacpatch patch  "/Applications/Black & White"     # apply
oldmacpatch revert "/Applications/Black & White"     # put it back
oldmacpatch scan -v <path>                           # show offsets
```

Point it at a folder and it walks it, examining every PowerPC PEF executable it
finds and ignoring everything else.

`patch` writes an undo record next to the executable as `<name>.omgpbak`
holding the original 48 bytes. `revert` uses it, then deletes it. Because the
patch is written in place and the undo record only stores the changed bytes,
nothing ever copies the file, so **resource forks are never at risk**. That
matters here: the game binaries carry 186 KB and 218 KB resource forks that a
naive `cp` or `scp` would silently discard.

Both operations re-read the file from disk afterwards and verify the result.

## Build

```
make                      # native
```

For a fat PowerPC and Intel build that runs on 10.4 and 10.5, see
[docs/BUILDING.md](docs/BUILDING.md).

## Black & White Display

<img src="icons/display-256.png" width="120" align="right" alt="">

The Mac port keeps its settings in an XML file that imitates the Windows
registry:

```
~/Library/Preferences/Lionhead/Black & White/Preferences Data
```

Resolution lives there as plain integers, so it does not need a binary patch at
all. The keys that matter:

| key | meaning |
|---|---|
| `ScreenW` / `ScreenH` | resolution in pixels |
| `ScreenD` | colour depth, 16 or 32 |
| `FullScreen` | 0 windowed, 1 fullscreen |
| `UseDesktopRes` | 1 to just take the desktop resolution |
| `UseDesktopDepth` | 1 to just take the desktop depth |
| `VSync` | 0 or 1 |
| `FSAAEnabled` / `FSAALevel` | anti-aliasing |

Setting `ScreenW`/`ScreenH` directly will go past what the setup screen's
resolution stepper offers. Prefer a mode the display actually supports: a
fullscreen switch to a non-native mode is the kind of thing that hangs some of
this hardware.

## Credits

Black & White is by Lionhead Studios. The Mac version is by Feral Interactive.
Creature Isle is its expansion. This project is not connected with either.

The Platinum Pack this was developed against is catalogued at
[Macintosh Garden](https://macintoshgarden.org/games/black-white-platinum-pack).

The icons are built by the pipeline from the
[old-mac-halflife](https://github.com/matthewdeaves) port: legacy ICNS chunks
plus a 256px `ic08`, which is the largest 10.3 Panther will accept before it
silently falls back to a generic icon.

## Scope

This fixes one crash. Other faults later in the game are possible, since the
same startup ordering may leave other pointers null and Tiger's layout would
hide those too. If you hit one, keep the crash log: it is traceable the same
way, and the write-up explains the method.
