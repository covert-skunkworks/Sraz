#!/usr/bin/env bash
# Pack a built Sraz for release: the program, the question banks, README.txt and the licence notices of what it carries
# (SDL2, stb_truetype, the Droid Sans font).
#
# usage, from the project's root after `make`:
#   scripts/package.sh OS VERSION [PROGRAM]     OS: windows, linux or macos
#
# Writes into dist/:
#
#   sraz-VERSION-windows-x64.zip       sraz.exe (SDL2 linked statically) + questions/
#   sraz-VERSION-linux-x64.tar.gz      tar keeps the program executable
#   sraz-VERSION-macos-universal.zip   Sraz.app (arm64 + x86_64), made with ditto, which keeps links and signature
#
# Each holds one folder of the same name. The font's licence (Apache 2.0) has to travel with the native programs; its
# text is fetched from apache.org unless APACHE_LICENSE names a local copy.

set -euo pipefail

os=${1:?usage: package.sh OS VERSION [PROGRAM]}
ver=${2:?usage: package.sh OS VERSION [PROGRAM]}
case "$os" in
	windows) arch=x64; prog=${3:-sraz.exe} ;;
	linux) arch=x64; prog=${3:-sraz} ;;
	macos) arch=universal; prog=${3:-sraz} ;;
	*) echo "package.sh: unknown OS $os" >&2; exit 1 ;;
esac
name="sraz-$ver-$os-$arch"
out="dist/$name"

rm -rf "$out" "dist/$name.zip" "dist/$name.tar.gz"
mkdir -p "$out/licenses"

case "$os" in
	windows) cp "$prog" "$out/sraz.exe"; cp -R questions "$out/questions" ;;
	linux) cp "$prog" "$out/sraz"; cp -R questions "$out/questions" ;;
	macos)
		app="$out/Sraz.app"
		mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
		cp "$prog" "$app/Contents/MacOS/sraz"
		cp -R questions "$app/Contents/Resources/questions"
		cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>                  <string>Sraz</string>
  <key>CFBundleDisplayName</key>           <string>Sraz</string>
  <key>CFBundleExecutable</key>            <string>sraz</string>
  <key>CFBundleIdentifier</key>            <string>local.sraz</string>
  <key>CFBundlePackageType</key>           <string>APPL</string>
  <key>CFBundleShortVersionString</key>    <string>$ver</string>
  <key>CFBundleVersion</key>               <string>$ver</string>
  <key>LSMinimumSystemVersion</key>        <string>11.0</string>
  <key>NSHighResolutionCapable</key>       <true/>
</dict>
</plist>
EOF
		codesign --force --deep --sign - "$app"
		;;
esac

cp release/README.txt "$out/README.txt"
[ -f LICENSE ] && cp LICENSE "$out/licenses/sraz.txt"
cp release/licenses/SDL2-zlib.txt "$out/licenses/SDL2-zlib.txt"
cp release/licenses/stb_truetype-public-domain.txt "$out/licenses/stb_truetype-public-domain.txt"
if [ -n "${APACHE_LICENSE:-}" ]; then
	cp "$APACHE_LICENSE" "$out/licenses/DroidSans-Apache-2.0.txt"
else
	curl -fsSL https://www.apache.org/licenses/LICENSE-2.0.txt -o "$out/licenses/DroidSans-Apache-2.0.txt"
fi

cd dist
case "$os" in
	windows)
		echo "dist/$name.zip:"
		if command -v zip > /dev/null; then
			zip -qr "$name.zip" "$name"
		else
			powershell.exe -NoProfile -Command "Compress-Archive -Path '$(cygpath -w "$PWD/$name")' -DestinationPath '$(cygpath -w "$PWD/$name.zip")'"
		fi
		;;
	linux)
		echo "dist/$name.tar.gz:"
		tar czf "$name.tar.gz" "$name"
		;;
	macos)
		echo "dist/$name.zip:"
		ditto -c -k --keepParent "$name" "$name.zip"
		;;
esac
find "$name" | sort | head -60
rm -rf "$name"
