#!/bin/sh
# Builds the release disc image. Run on a Tiger G4, per the same reasoning as
# the old-mac-halflife port: that is the machine whose hdiutil produces an image
# every target in the fleet mounts.
#
#   build/make-dmg.sh [version]
set -e
cd "$(dirname "$0")/.."

VER=${1:-1.0}
VOL="Black & White Mac Tools"
OUT="Black-and-White-Mac-Tools-$VER.dmg"
STAGE=/tmp/bwt-stage

[ -d dist ] || { echo "no dist/ - build the apps first" >&2; exit 1; }

rm -rf "$STAGE" "$OUT"
mkdir -p "$STAGE"
ditto --rsrc "dist/Black & White Patcher.app" "$STAGE/Black & White Patcher.app"
ditto --rsrc "dist/Black & White Display.app" "$STAGE/Black & White Display.app"
cp "dmg/Read Me.txt" "$STAGE/Read Me.txt"
mkdir -p "$STAGE/Command Line"
cp dist/oldmacpatch "$STAGE/Command Line/oldmacpatch"

hdiutil create -srcfolder "$STAGE" -volname "$VOL" -format UDZO -imagekey zlib-level=9 "$OUT"
rm -rf "$STAGE"

echo
echo "built $OUT"
hdiutil verify "$OUT"
ls -l "$OUT"
