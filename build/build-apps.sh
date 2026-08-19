#!/bin/sh
# Builds both apps fat (ppc + i386), 10.3.9 deployment target.
# Run on an Intel Mac with Xcode 4.6, which is the last toolchain targeting ppc.
set -e
cd "$(dirname "$0")/.."

SDK=${SDK:-/Developer/SDKs/MacOSX10.4u.sdk}
MIN=${MIN:-10.3.9}
CC=${CC:-gcc-4.0}
VER=${VER:-1.0.1}
OUT=dist

[ -d "$SDK" ] || { echo "SDK not found: $SDK" >&2; exit 1; }
rm -rf "$OUT"; mkdir -p "$OUT"

build_bin() {                 # $1=output  $2..=sources
    out=$1; shift
    for arch in ppc i386; do
        $CC -arch $arch -isysroot "$SDK" -mmacosx-version-min=$MIN -O2 -Wall \
            -framework Carbon -framework ApplicationServices \
            -o "/tmp/bwt-$arch" "$@" 2>&1 | grep -v 'mlong-branch' || true
        [ -f "/tmp/bwt-$arch" ] || { echo "build failed: $arch $out" >&2; exit 1; }
    done
    lipo -create /tmp/bwt-ppc /tmp/bwt-i386 -output "$out"
    rm -f /tmp/bwt-ppc /tmp/bwt-i386
}

make_app() {                  # $1=AppName $2=binary $3=icns $4=signature $5=docs?
    app="$OUT/$1.app"
    # The names contain "&", which is not legal raw in XML. An unescaped one
    # makes Info.plist unparseable, and the Finder then silently falls back to
    # the generic application icon.
    esc=$(printf '%s' "$1" | sed 's/&/\&amp;/g')
    mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
    cp "$2" "$app/Contents/MacOS/$1"
    chmod +x "$app/Contents/MacOS/$1"
    [ -f "$3" ] && cp "$3" "$app/Contents/Resources/"
    icon=$(basename "$3")
    docs=""
    if [ -n "$5" ]; then
      docs='	<key>CFBundleDocumentTypes</key>
	<array><dict>
		<key>CFBundleTypeName</key><string>Folder</string>
		<key>CFBundleTypeOSTypes</key><array><string>fold</string></array>
		<key>CFBundleTypeRole</key><string>Viewer</string>
		<key>LSItemContentTypes</key><array><string>public.folder</string></array>
	</dict></array>'
    fi
    cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>$esc</string>
	<key>CFBundleDisplayName</key><string>$esc</string>
	<key>CFBundleExecutable</key><string>$esc</string>
	<key>CFBundleIdentifier</key><string>uk.co.matthewdeaves.bwtools.$4</string>
	<key>CFBundleIconFile</key><string>$icon</string>
	<key>CFBundlePackageType</key><string>APPL</string>
	<key>CFBundleSignature</key><string>$4</string>
	<key>CFBundleVersion</key><string>$VER</string>
	<key>CFBundleShortVersionString</key><string>$VER</string>
	<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
	<key>LSMinimumSystemVersion</key><string>10.3.9</string>
	<key>NSHumanReadableCopyright</key><string>Free to use and share.</string>
$docs
</dict>
</plist>
EOF
    printf 'APPL%s' "$4" > "$app/Contents/PkgInfo"
    echo "  built $app"
    lipo -info "$app/Contents/MacOS/$1" | sed 's/^/    /'
}

echo "building command line tool"
build_bin "$OUT/oldmacpatch" src/md5.c src/pefpatch.c src/cli.c

echo "building apps"
build_bin /tmp/patcher-bin src/md5.c src/pefpatch.c src/patcher_app.c
make_app "Black & White Patcher" /tmp/patcher-bin icons/BlackAndWhitePatcher.icns BWPt docs

build_bin /tmp/display-bin src/registry.c src/display_app.c
make_app "Black & White Display" /tmp/display-bin icons/BlackAndWhiteDisplay.icns BWDs

rm -f /tmp/patcher-bin /tmp/display-bin
echo "done"
