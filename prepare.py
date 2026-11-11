#!/usr/bin/env python3
"""Copy the source-only starter, including its Haiven plugin, to a working directory.

The sibling haiven-sdk-unreal checkout, or HAIVEN_SDK_UNREAL, supplies ci/engines.json only.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUILD_ARTIFACTS = ("Binaries", "Intermediate", "Saved", "DerivedDataCache")


def unreal_root() -> Path:
    """Sibling haiven-sdk-unreal, or HAIVEN_SDK_UNREAL when that value is non-empty.

    That checkout is read for ci/engines.json only. Plugins/Haiven is the copy in this repository.
    """
    configured = os.environ.get("HAIVEN_SDK_UNREAL")
    if configured:
        return Path(configured).expanduser().resolve()
    return (HERE.parent / "haiven-sdk-unreal").resolve()


UNREAL = unreal_root()
MATRIX = json.loads((UNREAL / "ci/engines.json").read_text())


def engine(version: str) -> dict[str, object]:
    for candidate in MATRIX["engines"]:
        label = ".".join(str(part) for part in candidate["version"])
        if label == version:
            return candidate
    known = ", ".join(".".join(str(part) for part in row["version"]) for row in MATRIX["engines"])
    raise SystemExit(f"unsupported Unreal version {version}; choose {known}")


def _ignore(_directory: str, names: list[str]) -> set[str]:
    return set(shutil.ignore_patterns(*BUILD_ARTIFACTS)(_directory, names))


def assemble(destination: Path, version: str) -> Path:
    destination = destination.resolve()
    if destination.exists():
        raise SystemExit(f"destination already exists: {destination}")
    shutil.copytree(HERE, destination, ignore=_ignore)
    descriptor = destination / "HaivenChatStarter.uproject"
    project = json.loads(descriptor.read_text())
    project["EngineAssociation"] = engine(version)["association"]
    descriptor.write_text(json.dumps(project, indent="\t") + "\n")
    return destination


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    result.add_argument("--dest", type=Path, required=True, help="new working project directory")
    result.add_argument(
        "--engine", default=MATRIX["primary"], help="full engine version from engines.json"
    )
    return result


if __name__ == "__main__":
    arguments = parser().parse_args()
    print(assemble(arguments.dest, arguments.engine))
