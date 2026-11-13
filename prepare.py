#!/usr/bin/env python3
"""Copy the source-only starter, including its Haiven plugin, to a working directory.

Plugins/Haiven is the copy in this repository. HAIVEN_SDK_UNREAL does not replace it.
Engine versions are listed below, so a haiven-sdk-unreal checkout is not a plugin source.
"""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUILD_ARTIFACTS = ("Binaries", "Intermediate", "Saved", "DerivedDataCache")


# Engine version to the EngineAssociation the project needs. The first is primary.
ENGINES = {"5.8.2": "5.8", "5.6.1": "5.6"}
PRIMARY = "5.8.2"


def association(version: str) -> str:
    if version in ENGINES:
        return ENGINES[version]
    raise SystemExit(f"unsupported Unreal version {version}; choose {', '.join(ENGINES)}")


def _ignore(_directory: str, names: list[str]) -> set[str]:
    return set(shutil.ignore_patterns(*BUILD_ARTIFACTS)(_directory, names))


def assemble(destination: Path, version: str) -> Path:
    destination = destination.resolve()
    if destination.exists():
        raise SystemExit(f"destination already exists: {destination}")
    shutil.copytree(HERE, destination, ignore=_ignore)
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
