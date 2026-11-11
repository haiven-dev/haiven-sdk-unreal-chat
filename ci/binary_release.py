#!/usr/bin/env python3
"""Build the prebuilt plugin zips `.github/workflows/release-binaries.yml` publishes.

This is not the source archive. That zip is built by the source-release
workflow. Nothing here cooks a game or builds a Fab package.

A Linux host compiles both plugins for Linux. The editor target emits the
shared libraries in Binaries. Game targets are monolithic, so each plugin is
passed to UBT with -plugin= (the same switch BuildPlugin uses) and the shipped
game product is that module's .precompiled manifest plus its object files.
Unreal compiles Win64 only on Windows and Mac only on macOS, so those hosts
are described here and refused on Linux. They are not workflow jobs until a
runner exists.

The vendored Haiven plugin's VersionName stays 0.0.0. The release version is
HaivenChatStarter.uplugin's VersionName.

Usage:

    python3 ci/binary_release.py version [--tag v0.1.0]
    python3 ci/binary_release.py matrix
    python3 ci/binary_release.py binary --engine 5.8.2 --version 0.1.0 --platform Linux --out dist/
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform as host_platform
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
STARTER = REPO / "Plugins/HaivenChatStarter/HaivenChatStarter.uplugin"
HAIVEN = REPO / "Plugins/Haiven/Haiven.uplugin"
# The vendored plugin is not this repository's release. Do not bump it here.
HAIVEN_VERSION = "0.0.0"
VERSION = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z][0-9A-Za-z.-]*)?$")
EPOCH = (1980, 1, 1, 0, 0, 0)
PREFIX = "HaivenChatStarter/"
PROJECT_FILES = ("HaivenChatStarter.uproject", "README.md")
PLUGINS = ("Haiven", "HaivenChatStarter")
# Editor-only module. Game targets do not produce a binary for it.
EDITOR_MODULES = {"Haiven": ("Haiven", "HaivenEditor"), "HaivenChatStarter": ("HaivenChatStarter",)}
GAME_MODULES = {"Haiven": ("Haiven",), "HaivenChatStarter": ("HaivenChatStarter",)}
SKIP_DIRS = {"Saved", "DerivedDataCache", "HostProject", "__pycache__"}
# Object trees under Intermediate. Generated headers live in Inc, not here.
OBJECT_DIRS = {"Development", "Shipping", "Debug", "DebugGame", "Test"}
SKIP_SUFFIXES = {".debug", ".sym", ".pdb", ".rsp"}
# One UBT invocation. A cold runner also builds the plugin's editor target.
BUILD_TIMEOUT_SECONDS = 3000
# Platforms this script knows how to ask UBT for. Only Linux is hosted.
HOSTS = {"Linux": "Linux", "Win64": "Windows", "Mac": "Darwin"}
HOSTED = ("Linux",)
BUILD_TARGETS = (
    ("UnrealEditor", "Development"),
    ("UnrealGame", "Development"),
    ("UnrealGame", "Shipping"),
)


def load_engines() -> dict:
    return json.loads((HERE / "engines.json").read_text(encoding="utf-8"))


def engine_label(engine: dict) -> str:
    return ".".join(str(part) for part in engine["version"])


def read_descriptor(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def checked(version: str) -> str:
    if not VERSION.match(version):
        raise RuntimeError(f"version {version!r} is not <major>.<minor>.<patch>[-prerelease]")
    return version


def release_version(tag: str | None = None) -> str:
    declared = checked(read_descriptor(STARTER)["VersionName"])
    if not tag:
        return declared
    if not tag.startswith("v") or not VERSION.match(tag[1:]):
        raise RuntimeError(f"tag {tag!r} is not v<major>.<minor>.<patch>[-prerelease]")
    if tag[1:] != declared:
        raise RuntimeError(
            f"tag {tag} does not match {STARTER.name} VersionName {declared!r}; "
            f"bump VersionName to {tag[1:]!r} on the release branch before tagging"
        )
    return declared


def assert_vendored_haiven() -> None:
    found = read_descriptor(HAIVEN)["VersionName"]
    if found != HAIVEN_VERSION:
        raise RuntimeError(
            f"{HAIVEN.name} VersionName is {found!r}; this repository keeps the vendored plugin at {HAIVEN_VERSION}"
        )


def select_engine(requested: str) -> dict:
    engines = load_engines()["engines"]
    chosen = [engine for engine in engines if engine_label(engine) == requested]
    if len(chosen) != 1:
        known = ", ".join(engine_label(engine) for engine in engines)
        raise RuntimeError(f"unknown engine {requested!r}; engines.json declares {known}")
    return chosen[0]


def matrix() -> dict:
    """Linux rows only. Win64 and Mac need their own hosts; see require_host."""
    include = []
    for engine in load_engines()["engines"]:
        for hosted in HOSTED:
            include.append({
                "version": engine_label(engine),
                "ue": engine["association"],
                "label": engine["runnerLabel"],
                "root": engine["installPath"],
                "platform": hosted,
            })
    return {"include": include}


def require_host(platform: str) -> None:
    if platform not in HOSTS:
        raise RuntimeError(f"platform {platform!r} is not Linux, Win64, or Mac")
    found = host_platform.system()
    expected = HOSTS[platform]
    if found != expected:
        raise RuntimeError(
            f"platform {platform} is built only on {expected}; this host is {found}. "
            "Unreal does not cross-compile Win64 or Mac from Linux, and those runners are not jobs yet"
        )


def engine_root(engine: dict) -> Path:
    candidates = [os.environ.get(engine["rootEnv"]), os.environ.get("UNREAL_ENGINE_ROOT"), engine["installPath"]]
    expected = list(engine["version"])
    tried = []
    for candidate in candidates:
        if not candidate:
            continue
        root = Path(candidate).expanduser().resolve()
        tried.append(str(root))
        version_file = root / "Engine/Build/Build.version"
        if not version_file.is_file():
            continue
        found = json.loads(version_file.read_text(encoding="utf-8"))
        actual = [found.get(key) for key in ("MajorVersion", "MinorVersion", "PatchVersion")]
        if actual == expected:
            return root
    raise RuntimeError(
        f"no installed UE {engine_label(engine)} found; set {engine['rootEnv']} to a root "
        f"whose Engine/Build/Build.version matches (tried: {', '.join(tried) or 'nothing'})"
    )


def linux_toolchain(engine: dict, root: Path) -> Path:
    toolchain = root / "Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64" / engine["toolchain"]
    if not toolchain.is_dir():
        raise RuntimeError(f"UE {engine_label(engine)}: bundled toolchain {engine['toolchain']} is missing")
    return toolchain


def build_script(root: Path, platform: str) -> Path:
    relative = {
        "Linux": "Engine/Build/BatchFiles/Linux/Build.sh",
        "Win64": "Engine/Build/BatchFiles/Build.bat",
        "Mac": "Engine/Build/BatchFiles/Mac/Build.sh",
    }[platform]
    script = root / relative
    if not script.is_file():
        raise RuntimeError(f"missing {script}")
    return script


def plugin_build_order(target: str) -> tuple[str, ...]:
    """Haiven is the dependency of HaivenChatStarter.

    Editor builds the dependency first so the starter links against it. Game
    builds it last so Haiven.precompiled lists the objects from that final compile.
    """
    if target == "UnrealEditor":
        return PLUGINS
    return ("HaivenChatStarter", "Haiven")


def build_commands(root: Path, platform: str, project: Path) -> list[tuple[str, list[str]]]:
    """One UBT -plugin= invocation per plugin, for editor Development and both game configs.

    -plugin= is what marks the module bPrecompile and, on a monolithic game
    target, disables linking. Without it an installed engine tries to link
    UnrealGame and does not write the precompiled manifest. Nothing here cooks
    or invokes UAT. Both plugins are enabled in the staged project, so the
    starter can see Haiven on 5.6, which has no BuildPlugin -Dependencies switch.
    """
    script = build_script(root, platform)
    if platform == "Win64":
        prefix = [str(script)]
    else:
        prefix = ["bash", str(script)]
    commands = []
    for target, configuration in BUILD_TARGETS:
        for plugin in plugin_build_order(target):
            plugin_file = project.parent / "Plugins" / plugin / f"{plugin}.uplugin"
            label = f"{target}-{configuration}-{plugin}"
            commands.append((label, [
                *prefix,
                target,
                platform,
                configuration,
                f"-Project={project}",
                f"-plugin={plugin_file}",
                "-noubtmakefiles",
                "-nohotreload",
                "-WaitMutex",
            ]))
    return commands


def binary_names(platform: str) -> list[tuple[str, str]]:
    """Editor shared libraries a finished build must contain.

    Game configurations do not produce libUnrealGame-* libraries. require_binaries
    looks for <Module>.precompiled under the plugin instead.
    """
    if platform == "Linux":
        editor, extension = "libUnrealEditor-", ".so"
    elif platform == "Win64":
        editor, extension = "UnrealEditor-", ".dll"
    elif platform == "Mac":
        editor, extension = "libUnrealEditor-", ".dylib"
    else:
        raise RuntimeError(f"platform {platform!r} is not Linux, Win64, or Mac")
    names: list[tuple[str, str]] = []
    for plugin, modules in EDITOR_MODULES.items():
        for module in modules:
            names.append((plugin, f"Binaries/{platform}/{editor}{module}{extension}"))
    return names


def run_logged(command: list[str], log: Path, env: dict[str, str]) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as handle:
        process = subprocess.Popen(
            command, stdout=handle, stderr=subprocess.STDOUT, env=env, start_new_session=True
        )
        try:
            code = process.wait(BUILD_TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            raise RuntimeError(f"timed out after {BUILD_TIMEOUT_SECONDS}s; see {log}") from None
    if code != 0:
        tail = "\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-40:])
        raise RuntimeError(f"exit {code}; see {log}\n{tail}")


def stage_project(destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    for name in PROJECT_FILES:
        shutil.copy2(REPO / name, destination / name)
    shutil.copytree(REPO / "Config", destination / "Config")
    for plugin in PLUGINS:
        shutil.copytree(
            REPO / "Plugins" / plugin,
            destination / "Plugins" / plugin,
            ignore=shutil.ignore_patterns("Binaries", "Intermediate", "Saved", "DerivedDataCache", "__pycache__"),
        )


def write_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent="\t") + "\n", encoding="utf-8")


def stamp_association(project: Path, association: str) -> None:
    data = read_descriptor(project)
    data["EngineAssociation"] = association
    write_json(project, data)


def stamp_plugin(path: Path, association: str, version_name: str) -> None:
    """Pin the packaged copy to the engine that built it. VersionName is not a knob."""
    data = read_descriptor(path)
    if data.get("VersionName") != version_name:
        raise RuntimeError(f"packaged {path.name} VersionName is {data.get('VersionName')!r}, expected {version_name!r}")
    expected = f"{association}.0"
    found = data.get("EngineVersion")
    if found not in (None, expected):
        raise RuntimeError(f"packaged {path.name} declares EngineVersion {found!r}, expected {expected!r}")
    data["EngineVersion"] = expected
    data["Installed"] = True
    write_json(path, data)


def require_binaries(stage: Path, platform: str) -> None:
    stage = stage.resolve()
    missing: list[str] = []
    for plugin, relative in binary_names(platform):
        if not (stage / "Plugins" / plugin / relative).is_file():
            missing.append(f"Plugins/{plugin}/{relative}")
    for plugin, modules in GAME_MODULES.items():
        root = stage / "Plugins" / plugin
        manifests = sorted(root.rglob("*.precompiled")) if root.is_dir() else []
        for module in modules:
            for configuration in ("Development", "Shipping"):
                matched = [
                    path for path in manifests
                    if path.name == f"{module}.precompiled"
                    and "UnrealGame" in path.parts
                    and configuration in path.parts
                ]
                if len(matched) != 1:
                    missing.append(
                        f"Plugins/{plugin} {module} UnrealGame {configuration} "
                        f"precompiled manifest ({len(matched)})"
                    )
                    continue
                manifest = matched[0]
                try:
                    listed = json.loads(manifest.read_text(encoding="utf-8")).get("OutputFiles")
                except json.JSONDecodeError as error:
                    missing.append(f"{manifest.name}: {error}")
                    continue
                if not isinstance(listed, list) or not listed:
                    missing.append(f"{manifest.relative_to(stage).as_posix()} lists no object files")
                    continue
                for output in listed:
                    produced = (manifest.parent / str(output)).resolve()
                    try:
                        relative = produced.relative_to(stage)
                    except ValueError:
                        missing.append(f"{manifest.name} lists {output} outside the stage")
                        continue
                    if not produced.is_file():
                        missing.append(f"{manifest.name} lists missing {output}")
                    elif not include_file(relative):
                        missing.append(f"packager would drop {relative.as_posix()}")
    if missing:
        raise RuntimeError("build did not produce: " + ", ".join(missing))


def keep_directory(parent: tuple[str, ...], name: str) -> bool:
    if name in SKIP_DIRS or name.startswith("."):
        return False
    # Editor object trees are rebuilt from source. The editor product is the shared library.
    if "Intermediate" in parent and "UnrealEditor" in parent and name in OBJECT_DIRS:
        return False
    return True


def include_file(relative: Path) -> bool:
    parts = relative.parts
    if any(part.startswith(".") or part in SKIP_DIRS for part in parts):
        return False
    if relative.suffix in SKIP_SUFFIXES:
        return False
    if relative.suffix == ".precompiled":
        return True
    # Game precompile output. Editor .o files are not the installed editor binary.
    if relative.suffix in {".o", ".obj"}:
        return "Intermediate" in parts and "UnrealGame" in parts
    if "Intermediate" in parts and "Inc" not in parts:
        return False
    return True


def packaged_files(stage: Path) -> list[Path]:
    files = [stage / name for name in PROJECT_FILES]
    roots = [stage / "Config", *(stage / "Plugins" / plugin for plugin in PLUGINS)]
    for root in roots:
        for directory, subdirectories, names in os.walk(root):
            current = Path(directory)
            parent = () if current == root else current.relative_to(root).parts
            # Walk is rooted at Config or a plugin, so Intermediate's parents are
            # relative to that root. Prefix nothing: those trees start at their root.
            subdirectories[:] = sorted(name for name in subdirectories if keep_directory(parent, name))
            for name in sorted(names):
                path = current / name
                relative = path.relative_to(stage)
                if include_file(relative):
                    files.append(path)
    if not files:
        raise RuntimeError(f"nothing to package under {stage}")
    return files


def write_zip(stage: Path, archive: Path) -> Path:
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in packaged_files(stage):
            relative = path.relative_to(stage).as_posix()
            entry = zipfile.ZipInfo(PREFIX + relative, date_time=EPOCH)
            entry.compress_type = zipfile.ZIP_DEFLATED
            executable = path.suffix in {".so", ".dylib", ".sh"} or os.access(path, os.X_OK)
            mode = 0o755 if executable else 0o644
            entry.external_attr = (0o100000 | mode) << 16
            bundle.writestr(entry, path.read_bytes())
    return archive


def compile_staged(stage: Path, engine: dict, root: Path, platform: str, logs: Path) -> None:
    project = stage / "HaivenChatStarter.uproject"
    env = dict(os.environ)
    if platform == "Linux":
        env["LINUX_MULTIARCH_ROOT"] = str(linux_toolchain(engine, root))
    for index, (label, command) in enumerate(build_commands(root, platform, project), start=1):
        run_logged(command, logs / f"ubt-{index}-{label}.log", env)


def binary(requested: str, version: str, platform: str, out: Path, logs: Path) -> Path:
    version = checked(version)
    declared = release_version(None)
    if version != declared:
        raise RuntimeError(f"version {version!r} does not match {STARTER.name} VersionName {declared!r}")
    require_host(platform)
    assert_vendored_haiven()
    engine = select_engine(requested)
    root = engine_root(engine)
    logs.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"haiven-chat-binaries-{engine_label(engine)}-") as temporary:
        stage = Path(temporary) / "HaivenChatStarter"
        stage_project(stage)
        stamp_association(stage / "HaivenChatStarter.uproject", engine["association"])
        compile_staged(stage, engine, root, platform, logs)
        require_binaries(stage, platform)
        stamp_plugin(stage / "Plugins/Haiven/Haiven.uplugin", engine["association"], HAIVEN_VERSION)
        stamp_plugin(stage / "Plugins/HaivenChatStarter/HaivenChatStarter.uplugin", engine["association"], version)
        archive = Path(out).resolve() / f"haiven-unreal-chat-{version}-UE{engine['association']}-{platform}.zip"
        return write_zip(stage, archive)


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    version_parser = commands.add_parser("version")
    version_parser.add_argument("--tag")
    commands.add_parser("matrix")
    binary_parser = commands.add_parser("binary")
    binary_parser.add_argument("--engine", required=True)
    binary_parser.add_argument("--version", required=True)
    binary_parser.add_argument("--platform", default="Linux")
    binary_parser.add_argument("--out", required=True)
    binary_parser.add_argument("--logs", default=str(REPO / "build/release-logs"))
    args = parser.parse_args(argv)
    if args.command == "version":
        print(release_version(args.tag))
    elif args.command == "matrix":
        print(json.dumps(matrix(), separators=(",", ":")))
    elif args.command == "binary":
        print(binary(args.engine, args.version, args.platform, Path(args.out), Path(args.logs)))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        sys.exit(str(error))
