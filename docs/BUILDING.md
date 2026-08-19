# Building

## Native

```
make
```

Builds `oldmacpatch` for whatever machine you are on. C89, no dependencies.

## Fat PowerPC and Intel, for 10.4 and 10.5

The games are PowerPC, so the patcher wants to run on the Mac that has them.
Built on an Intel Mac running 10.7 with Xcode 4.6, which is the last toolchain
that can still target `ppc`:

```
build/build-fat.sh
```

That produces a `ppc` + `i386` binary with a 10.4 deployment target, which
covers 10.3.9 through 10.6, and runs under Rosetta on Intel.

Slices and floors follow the same reasoning as any old-Mac fat binary: `dyld`
grades a fat file by CPU subtype, so a slice only earns its place if it is a
real capability difference.
