# MyAIPs incremental updates

MyAIPs macOS updates may use a BSDIFF ZIP instead of a full DMG. The manifest
entry uses `package_type: "bsdiff-zip"`, the exact `base_version`, and a
`delta_download_url`. The ZIP contains `MyAIPsDelta/patches.tsv` and binary
patches. The table records the expected base and target SHA-256 for each changed
file.

The About updater downloads the ZIP, waits for MyAIPs to exit, verifies that the
installed bundle is the declared base version, copies the app locally, applies
patches to the copy, checks each base and target hash, verifies the target bundle
version and code signature, then swaps the staged app into place. A failed check
leaves the installed app untouched. The ZIP URL is not placed in the legacy
`download_url` field because older builds treat that URL as a complete DMG.

Build a delta with `scripts/release/make-myaips-delta.sh`, passing the installed
base bundle, the packaged target bundle, both versions, and the output ZIP path.
The build host needs Python 3 with `bsdiff4` and the macOS `bspatch` tool. The
script rejects added, removed, renamed, or symlink-changed bundle files rather
than producing an incomplete patch. Installed clients use macOS's system
`/usr/bin/bspatch`, so the download never executes a helper from the archive.

Each delta applies only to its declared base version. The release process must
keep the user's app on the preceding delta-capable version before publishing the
next manifest entry. A client older than the first delta-capable build cannot
install a delta package; it needs a one-time local bootstrap before the feed points
to delta-only releases.
