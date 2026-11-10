#!/usr/bin/env python3
"""Build the source archive `.github/workflows/release.yml` publishes.

The archive is the chat starter project. It is not a cooked game and not a
Fab package. Its single top-level folder is `HaivenChatStarter/`.

Usage:

    python3 ci/release.py version [--tag v0.1.0]
        Print the release version. With --tag, the tag must be `v` plus the
        `VersionName` in `Plugins/HaivenChatStarter/HaivenChatStarter.uplugin`.
        Without it, that VersionName is the version (dry runs).

    python3 ci/release.py source --version 0.1.0 --out dist/
        Write `haiven-unreal-chat-source-<version>.zip`. The version must
        match the starter descriptor. The zip contains the uproject, Config,
        Plugins, and README, and omits build directories.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys
import zipfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
DESCRIPTOR = REPO / "Plugins/HaivenChatStarter/HaivenChatStarter.uplugin"
# The project a user opens. prepare.py stays out: it copies a sibling checkout
# and is not part of the source archive.
INCLUDED = ("HaivenChatStarter.uproject", "README.md", "Config", "Plugins")
# Build products and editor state that must never ship in a source bundle.
EXCLUDED_DIRS = {"Binaries", "Intermediate", "Saved", "DerivedDataCache", "__pycache__"}
# Semantic version with an optional pre-release suffix: 1.2.3 or 1.2.3-rc.1.
VERSION = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z][0-9A-Za-z.-]*)?$")
# Fixed timestamp so the same tree always produces the same zip bytes.
EPOCH = (1980, 1, 1, 0, 0, 0)
PREFIX = "HaivenChatStarter/"


def descriptor_version() -> str:
    return json.loads(DESCRIPTOR.read_text(encoding="utf-8-sig"))["VersionName"]


def checked(version: str) -> str:
    if not VERSION.match(version):
        raise RuntimeError(f"version {version!r} is not <major>.<minor>.<patch>[-prerelease]")
    return version


def release_version(tag: str | None = None) -> str:
    declared = checked(descriptor_version())
    if not tag:
        return declared
    if not tag.startswith("v") or not VERSION.match(tag[1:]):
        raise RuntimeError(f"tag {tag!r} is not v<major>.<minor>.<patch>[-prerelease]")
    if tag[1:] != declared:
        raise RuntimeError(
            f"tag {tag} does not match {DESCRIPTOR.name} VersionName {declared!r}; "
            f"bump VersionName to {tag[1:]!r} on main before tagging"
        )
    return declared


def project_files(root: Path) -> list[Path]:
    """Files that belong in the source archive, sorted, with build dirs skipped."""
    files: list[Path] = []
    for name in INCLUDED:
        path = root / name
        if path.is_file():
            files.append(path)
            continue
        if not path.is_dir():
            raise RuntimeError(f"missing {name}")
        for directory, subdirectories, names in os.walk(path):
            subdirectories[:] = sorted(
                item for item in subdirectories if item not in EXCLUDED_DIRS and not item.startswith(".")
            )
            for filename in sorted(names):
                if filename.startswith("."):
                    continue
                files.append(Path(directory) / filename)
    if not files:
        raise RuntimeError(f"nothing to package under {root}")
    return files


def write_zip(root: Path, archive: Path) -> Path:
    """Zip the starter project as a top-level `HaivenChatStarter/` folder."""
    archive.parent.mkdir(parents=True, exist_ok=True)
    files = project_files(root)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in files:
            entry = zipfile.ZipInfo(PREFIX + path.relative_to(root).as_posix(), date_time=EPOCH)
            entry.compress_type = zipfile.ZIP_DEFLATED
            mode = 0o755 if os.access(path, os.X_OK) else 0o644
            entry.external_attr = (0o100000 | mode) << 16
            bundle.writestr(entry, path.read_bytes())
    return archive


def source(version: str, out: str | Path) -> Path:
    version = checked(version)
    declared = release_version(None)
    if version != declared:
        raise RuntimeError(
            f"version {version!r} does not match {DESCRIPTOR.name} VersionName {declared!r}"
        )
    return write_zip(REPO, Path(out).resolve() / f"haiven-unreal-chat-source-{version}.zip")


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    version_parser = commands.add_parser("version")
    version_parser.add_argument("--tag")
    source_parser = commands.add_parser("source")
    source_parser.add_argument("--version", required=True)
    source_parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    if args.command == "version":
        print(release_version(args.tag))
    elif args.command == "source":
        print(source(args.version, args.out))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        sys.exit(str(error))
