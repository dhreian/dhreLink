#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Darwin ]]; then
  echo "Build the OBS Receiver on macOS." >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"
template_dir="$repo_dir/.deps/obs-plugintemplate"
template_commit=3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc
build_dir="$repo_dir/build/macos-obs"

if [[ ! -d "$template_dir/.git" ]]; then
  mkdir -p "$template_dir"
  git -C "$template_dir" init
  git -C "$template_dir" remote add origin https://github.com/obsproject/obs-plugintemplate.git
  git -C "$template_dir" fetch --depth 1 origin "$template_commit"
  git -C "$template_dir" checkout --detach FETCH_HEAD
fi

if [[ "$(git -C "$template_dir" rev-parse HEAD)" != "$template_commit" ]]; then
  echo "The OBS plugin template checkout is not at the pinned commit." >&2
  exit 1
fi

cp "$repo_dir/src/obs/macos/buildspec.json" "$template_dir/buildspec.json"
cp "$repo_dir/src/obs/macos/CMakeLists.txt" "$template_dir/CMakeLists.txt"
version="$(tr -d '\r\n' < "$repo_dir/packaging/macos/VERSION")"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid macOS version in packaging/macos/VERSION." >&2
  exit 1
fi
python3 - "$template_dir/buildspec.json" "$version" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
spec = json.loads(path.read_text(encoding="utf-8"))
spec["version"] = sys.argv[2]
path.write_text(json.dumps(spec, indent=4) + "\n", encoding="utf-8")
PY

cmake -S "$template_dir" -B "$build_dir" -G Xcode \
  "-DDHRELINK_SOURCE_ROOT=$repo_dir" \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$build_dir" --config Release --target dhreLink --parallel 3

bundle="$build_dir/Release/dhreLink.plugin"
if [[ ! -d "$bundle" ]]; then
  echo "OBS bundle was not found at $bundle" >&2
  exit 1
fi
binary="$bundle/Contents/MacOS/dhreLink"
lipo "$binary" -verify_arch arm64 x86_64
codesign --force --deep --sign - "$bundle"
codesign --verify --deep --strict "$bundle"

dist_dir="$repo_dir/dist"
mkdir -p "$dist_dir"
(cd "$build_dir/Release" && ditto -c -k --sequesterRsrc --keepParent dhreLink.plugin \
  "$dist_dir/dhreLink-Receiver-macos-universal-unsigned.zip")
echo "OBS Receiver: $dist_dir/dhreLink-Receiver-macos-universal-unsigned.zip"
