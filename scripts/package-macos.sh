#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Darwin || "$#" -ne 2 ]]; then
  echo "Usage (macOS): package-macos.sh <Sender.vst3> <Receiver.plugin>" >&2
  exit 1
fi

sender="$1"
receiver="$2"
if [[ ! -d "$sender" || ! -d "$receiver" ]]; then
  echo "Both the VST3 Sender and OBS Receiver bundles are required." >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"
dist_dir="$repo_dir/dist"
stage="$(mktemp -d "${TMPDIR:-/tmp}/dhrelink-macos.XXXXXX")"
trap 'rm -rf "$stage"' EXIT

sender_binary="$sender/Contents/MacOS/dhreLink Sender"
receiver_binary="$receiver/Contents/MacOS/dhreLink"
for binary in "$sender_binary" "$receiver_binary"; do
  lipo "$binary" -verify_arch arm64 x86_64
done

# Re-sign after the VST3 SDK's Release strip step. Ad-hoc signing is free.
for bundle in "$sender" "$receiver"; do
  codesign --force --deep --sign - "$bundle"
  codesign --verify --deep --strict "$bundle"
done

mkdir -p "$stage/root/Library/Audio/Plug-Ins/VST3"
mkdir -p "$stage/root/Library/Application Support/obs-studio/plugins"
ditto "$sender" "$stage/root/Library/Audio/Plug-Ins/VST3/dhreLink Sender.vst3"
ditto "$receiver" "$stage/root/Library/Application Support/obs-studio/plugins/dhreLink.plugin"

identifier=com.dhreian.dhrelink.macos
version="$(tr -d '\r\n' < "$repo_dir/packaging/macos/VERSION")"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid macOS version in packaging/macos/VERSION." >&2
  exit 1
fi
pkgbuild --root "$stage/root" --install-location / \
  --identifier "$identifier" --version "$version" "$stage/dhreLink-component.pkg"

cat > "$stage/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>dhreLink $version</title>
  <options hostArchitectures="arm64,x86_64" customize="never"/>
  <domains enable_currentUserHome="true" enable_anywhere="false" enable_localSystem="false"/>
  <choices-outline><line choice="dhrelink"/></choices-outline>
  <choice id="dhrelink" title="dhreLink Sender and Receiver">
    <pkg-ref id="$identifier"/>
  </choice>
  <pkg-ref id="$identifier" version="$version">dhreLink-component.pkg</pkg-ref>
</installer-gui-script>
EOF

mkdir -p "$dist_dir"
base="dhreLink-$version-macos-universal-unsigned"
productbuild --distribution "$stage/distribution.xml" --package-path "$stage" \
  "$dist_dir/$base.pkg"
(cd "$stage/root" && ditto -c -k --sequesterRsrc . "$dist_dir/$base.zip")
(cd "$dist_dir" && shasum -a 256 "$base.pkg" "$base.zip" > "$base.sha256")

echo "Installer: $dist_dir/$base.pkg"
echo "Manual ZIP: $dist_dir/$base.zip"
