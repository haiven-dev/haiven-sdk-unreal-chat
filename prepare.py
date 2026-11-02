#!/usr/bin/env python3
"""Copy the source-only starter to a working directory and install the Haiven plugin."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
SDK = HERE.parents[1]
MATRIX = json.loads((SDK / "sdks/unreal/ci/engines.json").read_text())
BUILD_ARTIFACTS = ("Binaries", "Intermediate", "Saved", "DerivedDataCache")


def engine(version: str) -> dict[str, object]:
    for candidate in MATRIX["engines"]:
        label = ".".join(str(part) for part in candidate["version"])
        if label == version:
            return candidate
    known = ", ".join(".".join(str(part) for part in row["version"]) for row in MATRIX["engines"])
    raise SystemExit(f"unsupported Unreal version {version}; choose {known}")


def assemble(destination: Path, version: str) -> Path:
    destination = destination.resolve()
    if destination.exists():
        raise SystemExit(f"destination already exists: {destination}")
    ignore = shutil.ignore_patterns(*BUILD_ARTIFACTS, "Plugins/Haiven")
    shutil.copytree(HERE, destination, ignore=ignore)
    shutil.copytree(SDK / "sdks/unreal/Haiven", destination / "Plugins/Haiven", ignore=ignore)
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
