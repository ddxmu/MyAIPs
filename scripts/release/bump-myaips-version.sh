#!/usr/bin/env bash
# Increment the MyAIPs release version. Two-part versions intentionally use
# decimal-looking minor values: 0.99 -> 0.100 -> 0.101.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CMAKE_FILE="$ROOT/CMakeLists.txt"
MANIFEST_FILE="$ROOT/latest_version.json"

current=$(python3 - "$CMAKE_FILE" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text()
match = re.search(r'project\(\s*Patchy\s+VERSION\s+([0-9]+(?:\.[0-9]+){1,2})', text, re.S)
if not match:
    raise SystemExit('Could not find project version in CMakeLists.txt')
print(match.group(1))
PY
)

next_version=$(python3 - "$current" <<'PY'
import sys

parts = [int(part) for part in sys.argv[1].split('.')]
parts[-1] += 1
print('.'.join(str(part) for part in parts))
PY
)

if [[ "${1:-}" == "--dry-run" ]]; then
  printf '%s -> %s\n' "$current" "$next_version"
  exit 0
fi

python3 - "$CMAKE_FILE" "$MANIFEST_FILE" "$current" "$next_version" <<'PY'
import json
import re
import sys
from pathlib import Path

cmake_path, manifest_path, current, next_version = map(Path, sys.argv[1:])
text = cmake_path.read_text()
pattern = r'(project\(\s*Patchy\s+VERSION\s+)' + re.escape(str(current))
updated, count = re.subn(pattern, r'\g<1>' + str(next_version), text, count=1, flags=re.S)
if count != 1:
    raise SystemExit('CMakeLists.txt version changed while bumping')
cmake_path.write_text(updated)

manifest = json.loads(manifest_path.read_text())
platforms = manifest.get('platforms', {})
if not platforms:
    raise SystemExit('latest_version.json has no platform entries')
for entry in platforms.values():
    if isinstance(entry, dict):
        entry['version'] = str(next_version)
        download_url = entry.get('download_url')
        if isinstance(download_url, str):
            entry['download_url'] = (download_url
                                     .replace(f'/v{current}/', f'/v{next_version}/')
                                     .replace(f'-{current}.', f'-{next_version}.'))
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
PY

printf '%s\n' "$next_version"
