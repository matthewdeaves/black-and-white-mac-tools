# old-mac-game-patcher

Makes **Black & White** and **Creature Isle** for Mac run on **Mac OS X 10.5
Leopard**, where they otherwise crash a few seconds after launch.

The games work on 10.4 Tiger. On 10.5 they die during the opening logo movies
with `EXC_BAD_ACCESS (SIGBUS)` at address `0x6898`. This tool fixes that by
writing 48 bytes into the game's executable, and it can put them back.

It ships no game content. It patches a copy of the game you already own.

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

## Credits

Black & White is by Lionhead Studios. The Mac version is by Feral Interactive.
Creature Isle is its expansion. This project is not connected with either.

The Platinum Pack this was developed against is catalogued at
[Macintosh Garden](https://macintoshgarden.org/games/black-white-platinum-pack).

## Scope

This fixes one crash. Other faults later in the game are possible, since the
same startup ordering may leave other pointers null and Tiger's layout would
hide those too. If you hit one, keep the crash log: it is traceable the same
way, and the write-up explains the method.
