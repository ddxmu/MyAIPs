#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "Usage: $0 <base.app> <target.app> <base-version> <target-version> <output.zip>" >&2
  exit 2
fi

BASE_APP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
TARGET_APP=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
BASE_VERSION=$3
TARGET_VERSION=$4
OUTPUT=$(mkdir -p "$(dirname "$5")" && cd "$(dirname "$5")" && pwd)/$(basename "$5")

[[ -d "$BASE_APP" && -d "$TARGET_APP" ]] || { echo "Both app bundles must exist." >&2; exit 1; }
[[ "$BASE_VERSION" != "$TARGET_VERSION" ]] || { echo "Base and target versions must differ." >&2; exit 1; }
BASE_APP_VERSION=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$BASE_APP/Contents/Info.plist")
TARGET_APP_VERSION=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$TARGET_APP/Contents/Info.plist")
[[ "$BASE_APP_VERSION" == "$BASE_VERSION" ]] || { echo "Base app version is $BASE_APP_VERSION, expected $BASE_VERSION." >&2; exit 1; }
[[ "$TARGET_APP_VERSION" == "$TARGET_VERSION" ]] || { echo "Target app version is $TARGET_APP_VERSION, expected $TARGET_VERSION." >&2; exit 1; }
[[ -x /usr/bin/bspatch ]] || { echo "macOS bspatch is required." >&2; exit 1; }
python3 -c 'import bsdiff4' >/dev/null 2>&1 || {
  echo "Python module bsdiff4 is required to build the delta package." >&2
  exit 1
}

WORK=$(mktemp -d "${TMPDIR:-/tmp}/myaips-delta.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/MyAIPsDelta/patches"

python3 - "$BASE_APP" "$TARGET_APP" "$WORK/MyAIPsDelta" "$OUTPUT" <<'PY'
import hashlib
import os
import stat
import sys
import zipfile
from pathlib import Path

import bsdiff4

base_app, target_app, package_root, output = map(Path, sys.argv[1:])


def inventory(root: Path):
    files = {}
    links = {}
    for directory, names, filenames in os.walk(root, followlinks=False):
        directory_path = Path(directory)
        names[:] = sorted(names)
        for name in list(names):
            path = directory_path / name
            relative = path.relative_to(root).as_posix()
            if path.is_symlink():
                links[relative] = os.readlink(path)
                names.remove(name)
        for name in sorted(filenames):
            path = directory_path / name
            relative = path.relative_to(root).as_posix()
            if path.is_symlink():
                links[relative] = os.readlink(path)
            elif path.is_file():
                files[relative] = path
    return files, links


base_files, base_links = inventory(base_app)
target_files, target_links = inventory(target_app)
if set(base_files) != set(target_files) or base_links != target_links:
    raise SystemExit("App bundle file and symlink inventories must match for a delta update.")

manifest = []
patch_bytes = 0
for relative in sorted(base_files):
    if not relative.startswith("Contents/") or not relative.isascii() or any(part in {".", ".."} for part in relative.split("/")):
        raise SystemExit(f"Unsupported bundle path: {relative!r}")
    if stat.S_IMODE(base_files[relative].stat().st_mode) != stat.S_IMODE(target_files[relative].stat().st_mode):
        raise SystemExit(f"Changed file permissions are not supported: {relative}")
    old = base_files[relative].read_bytes()
    new = target_files[relative].read_bytes()
    if old == new:
        continue
    patch_relative = f"patches/{relative}.bsdiff"
    patch_path = package_root / patch_relative
    patch_path.parent.mkdir(parents=True, exist_ok=True)
    patch = bsdiff4.diff(old, new)
    patch_path.write_bytes(patch)
    manifest.append(
        "\t".join(
            (
                hashlib.sha256(old).hexdigest(),
                hashlib.sha256(new).hexdigest(),
                relative,
                patch_relative,
            )
        )
    )
    patch_bytes += len(patch)

if not manifest:
    raise SystemExit("The target bundle contains no changed files.")
(package_root / "patches.tsv").write_text("\n".join(manifest) + "\n", encoding="ascii")

output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in sorted(package_root.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(package_root.parent).as_posix()
        mode = stat.S_IMODE(path.stat().st_mode)
        info = zipfile.ZipInfo(relative)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (stat.S_IFREG | mode) << 16
        archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)

digest = hashlib.sha256(output.read_bytes()).hexdigest()
print(f"Delta files: {len(manifest)}")
print(f"BSDIFF data: {patch_bytes:,} bytes")
print(f"Package: {output} ({output.stat().st_size:,} bytes)")
print(f"SHA-256: {digest}")
PY
