# Black & White on Mac OS X 10.5: the crash, and the fix

Measured on a Power Mac G5 Quad (PowerMac11,2, ppc970MP, GeForce 6600), booting
the same disk into 10.4.11 Tiger and 10.5.8 Leopard, running the **same bytes**
of the game from the same partition in both.

## Symptom

Black & White 1.1.9 dies a few seconds after launch on 10.5.8, every time.
Creature Isle does the same. Five consecutive crash logs are identical:

```
Exception Type:  EXC_BAD_ACCESS (SIGBUS)
Exception Codes: KERN_PROTECTION_FAILURE at 0x0000000000006898
Crashed Thread:  0
0   ???       0x011e67e0
1   ???       0x01625258
2   com.apple.HIToolbox  DispatchEventToHandlers(...)
```

Two things stand out. The fault address is **the same constant every time**,
including in Creature Isle, which is a different executable. And every frame in
the application itself is `???`, because Apple's crash reporter cannot
symbolise a CFM binary.

## Symbolising it

The game is a PEF (CFM) executable launched by `LaunchCFMApp`, not a Mach-O.
It still carries its PowerPC **traceback tables**, the metadata the compiler
emits after each function body: a zero word, some flags, then a length-prefixed
name. Reading those out of the code section recovers 17,973 symbols for Black &
White and 18,370 for Creature Isle, with full C++ mangling.

Mapping the stack through them:

```
main -> DoGameLoop -> pc_main -> PlayLogoScreens
  -> PlayFullScreenMovie(const char*, const Rect&)
    -> CallBackContinueMovie() -> DoMacEventLoop()
      -> [HIToolbox dispatch]
        -> HandleApplicationEvents
          -> DoEventMouseDown(OpaqueEventRef*, double)
            -> SetMouseModFlags(unsigned long)
              -> BindableAction::GetPrimaryKey() const     <- faults
```

So: a mouse-down during the opening logo movies.

## The instruction

The PEF code maps at a clean `0x01000000`, so a crash address maps to a file
offset directly. Searching the code section for a load with base `r4` and
displacement `0x100`, constrained to offsets where the implied load base is page
aligned, returns **exactly one** candidate out of 7.3 MB of code:

```
0x011e67e0  80a40100  lwz r5,0x100(r4)
```

That is the first instruction of `GetPrimaryKey`. Its caller shows where the
pointer came from:

```
0x01627cb0  lwz   r4,0(r30)        ; global manager
0x01627cb8  addis r4,r4,0x25
0x01627cbc  lwz   r4,-12828(r4)    ; member pointer -> NULL
0x01627cc0  addi  r4,r4,26520      ; + 0x6798
0x01627cc4  bl    GetPrimaryKey
```

26520 is `0x6798`. The member pointer loaded at `0x01627cbc` is null, so `this`
becomes `0 + 0x6798`, and the first instruction reads `0x6798 + 0x100 = 0x6898`.
The registers agree: `r4 = 0x6798`, `dar = 0x6898`, `r31 = 0`.

A null pointer dereference with a large member offset. The address is a constant
because both the null base and the offset are constants, which is why two
different executables fault at the same place.

## Why 10.4 survives it

`vmmap` on the live, working process under Tiger:

```
__LINKEDIT      00004000-00006000  r--       LaunchCFMApp
shared memory   00006000-00008000  r--/rw-   SM=SHM
```

`0x6898` lands inside an 8 KB readable shared-memory region that the CFM runtime
maps at the first free address above `LaunchCFMApp`. The bad read succeeds,
returns junk, and the game carries on. Neither Finder nor a plain BSD process
has that region, so it is specific to the CFM process layout.

Under Leopard the same address is not readable, and `KERN_PROTECTION_FAILURE`
says the page is mapped but the access was refused. Both systems' `LaunchCFMApp`
binaries have byte-identical segment layouts, so what differs is what the OS
places at `0x6000`.

The game reads off a null pointer on both systems. Only one of them has
something harmless sitting there.

## The fix

A guard on the front of `GetPrimaryKey`, in the dead traceback table of the
function before it:

```
        cmplwi r4,0xffff        ; this < 0x10000 => null base + member offset
        bgt    normal
        li     r0,66            ; the return value is 264 bytes = 66 words
        mtctr  r0
        addi   r5,r3,-4         ; r3 = hidden struct-return buffer
        li     r0,0
loop:   stwu   r0,4(r5)
        bdnz   loop
        blr                     ; return an all-zero key
normal: lwz    r5,0x100(r4)     ; displaced original instruction
        b      entry+4
...
entry:  b      guard            ; was: lwz r5,0x100(r4)
```

Every branch is PC relative, so the same 44 bytes work at any cave that sits
exactly `0x38` below the entry.

Zero-filling rather than returning immediately matters: the caller reads the
returned struct straight away with `lwz r29,320(r1)`. On Tiger it currently gets
whatever junk is at `0x6898`, so zeros are strictly more deterministic than what
the game already survives.

## Result

Patched, Black & White gets past the logo movies on 10.5.8 and plays. The same
48-byte patch is applied to Creature Isle at its own offsets.

## Notes for anyone extending this

- The fault address being **constant across two executables** was the clue that
  it was a fixed offset off a null base, not a wild pointer.
- `KERN_PROTECTION_FAILURE` rather than `KERN_INVALID_ADDRESS` says the page
  exists. That is what sent me to `vmmap` on the working system, which is where
  the answer was.
- Traceback tables are the single most useful thing in these binaries and
  nothing at runtime reads them, which is what makes them safe to overwrite.
- Not every old-Mac failure is patchable. Crash logs from Giants on the same
  machine show `EXC_BREAKPOINT` inside dyld's address range, which is a linkage
  failure, and no byte patch fixes that.
