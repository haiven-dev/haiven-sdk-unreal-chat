#!/usr/bin/env python3
"""Copy the source-only starter to a working directory and install the Haiven plugin."""

from __future__ import annotations

import argparse
import json
import os
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUILD_ARTIFACTS = ("Binaries", "Intermediate", "Saved", "DerivedDataCache")


# Engine version to the EngineAssociation the project needs. The first is primary.
ENGINES = {"5.8.2": "5.8", "5.6.1": "5.6"}
PRIMARY = "5.8.2"


def plugin_source() -> Path:
    """HAIVEN_SDK_UNREAL/Haiven when that value is non-empty, else the plugin in this repository."""
    configured = os.environ.get("HAIVEN_SDK_UNREAL")
    if configured:
        return Path(configured).expanduser().resolve() / "Haiven"
    return HERE / "Plugins/Haiven"


PLUGIN = plugin_source()


def association(version: str) -> str:
    if version in ENGINES:
        return ENGINES[version]
    raise SystemExit(f"unsupported Unreal version {version}; choose {', '.join(ENGINES)}")


def _ignore(directory: str, names: list[str], *, skip_plugin: bool) -> set[str]:
    ignored = set(shutil.ignore_patterns(*BUILD_ARTIFACTS)(directory, names))
    # ignore_patterns matches the entry name, so "Plugins/Haiven" never matches.
    if skip_plugin and Path(directory).name == "Plugins":
        ignored.update(name for name in names if name == "Haiven")
    return ignored


def assemble(destination: Path, version: str) -> Path:
    destination = destination.resolve()
    if destination.exists():
        raise SystemExit(f"destination already exists: {destination}")
    shutil.copytree(HERE, destination, ignore=lambda directory, names: _ignore(directory, names, skip_plugin=True))
    shutil.copytree(
        PLUGIN,
        destination / "Plugins/Haiven",
        ignore=lambda directory, names: _ignore(directory, names, skip_plugin=False),
    )
    descriptor = destination / "HaivenChatStarter.uproject"
    project = json.loads(descriptor.read_text())
    project["EngineAssociation"] = association(version)
    descriptor.write_text(json.dumps(project, indent="\t") + "\n")
    return destination


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    result.add_argument("--dest", type=Path, required=True, help="new working project directory")
    result.add_argument(
        "--engine", default=PRIMARY, help=f"full engine version: {', '.join(ENGINES)}"
    )
    return result


if __name__ == "__main__":
    arguments = parser().parse_args()
    print(assemble(arguments.dest, arguments.engine))
