import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("haiven_chat_binary", Path(__file__).with_name("binary_release.py"))
binary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(binary)


class VersionTests(unittest.TestCase):
    def test_checked_rejects_path_like_versions(self):
        for version in ("../1.2.3", "1.2.3/x", "", "0.1"):
            with self.subTest(version=version), self.assertRaises(RuntimeError):
                binary.checked(version)

    def test_tag_must_match_starter_descriptor(self):
        self.assertEqual(binary.release_version(None), "0.1.0")
        self.assertEqual(binary.release_version("v0.1.0"), "0.1.0")
        for tag in ("v0.1.1", "0.1.0", "v0.1", "v0.1.0-preview.1", "vx.y.z"):
            with self.subTest(tag=tag), self.assertRaises(RuntimeError):
                binary.release_version(tag)

    def test_vendored_haiven_stays_at_0_0_0(self):
        self.assertEqual(binary.HAIVEN_VERSION, "0.0.0")
        self.assertEqual(binary.read_descriptor(binary.HAIVEN)["VersionName"], "0.0.0")
        self.assertFalse(binary.read_descriptor(binary.HAIVEN)["Installed"])
        self.assertNotIn("EngineVersion", binary.read_descriptor(binary.HAIVEN))
        binary.assert_vendored_haiven()

    def test_binary_refuses_a_version_that_does_not_match(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(RuntimeError):
            binary.binary("5.8.2", "9.9.9", "Linux", Path(directory), Path(directory))


class MatrixTests(unittest.TestCase):
    def test_matrix_is_linux_for_each_engine(self):
        engines = json.loads((binary.HERE / "engines.json").read_text(encoding="utf-8"))["engines"]
        include = binary.matrix()["include"]
        self.assertEqual([row["platform"] for row in include], ["Linux", "Linux"])
        self.assertEqual([row["label"] for row in include], [engine["runnerLabel"] for engine in engines])
        self.assertEqual([row["ue"] for row in include], [engine["association"] for engine in engines])
        self.assertEqual([row["version"] for row in include], ["5.8.2", "5.6.1"])
        self.assertNotIn("Win64", {row["platform"] for row in include})
        self.assertNotIn("Mac", {row["platform"] for row in include})

    def test_win64_and_mac_are_refused_on_linux(self):
        for platform in ("Win64", "Mac", "Android"):
            with self.subTest(platform=platform), self.assertRaises(RuntimeError):
                binary.require_host(platform)

    def test_linux_commands_are_editor_and_game_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / "Engine/Build/BatchFiles/Linux/Build.sh"
            script.parent.mkdir(parents=True)
            script.write_text("#!/bin/bash\n")
            project = root / "HaivenChatStarter.uproject"
            commands = binary.build_commands(root, "Linux", project)
        rendered = [" ".join(argv) for _label, argv in commands]
        self.assertEqual(len(commands), 6)
        editor = [command for command in rendered if "UnrealEditor Linux Development" in command]
        game_development = [command for command in rendered if "UnrealGame Linux Development" in command]
        game_shipping = [command for command in rendered if "UnrealGame Linux Shipping" in command]
        self.assertEqual(len(editor), 2)
        self.assertIn("Haiven.uplugin", editor[0])
        self.assertIn("HaivenChatStarter.uplugin", editor[1])
        self.assertIn("HaivenChatStarter.uplugin", game_development[0])
        self.assertIn("Haiven.uplugin", game_development[1])
        self.assertIn("HaivenChatStarter.uplugin", game_shipping[0])
        self.assertIn("Haiven.uplugin", game_shipping[1])
        self.assertTrue(all(f"-Project={project}" in command and "-plugin=" in command for command in rendered))
        self.assertTrue(all("-noubtmakefiles" in command and "-nohotreload" in command for command in rendered))
        self.assertTrue(all("BuildCookRun" not in command and "RunUAT" not in command for command in rendered))
        self.assertNotIn("cook", " ".join(rendered).lower())


class PackageTests(unittest.TestCase):
    def test_archive_keeps_versions_and_drops_build_junk(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "stage"
            self.write_stage(stage)
            binary.stamp_association(stage / "HaivenChatStarter.uproject", "5.8")
            binary.stamp_plugin(stage / "Plugins/Haiven/Haiven.uplugin", "5.8", "0.0.0")
            binary.stamp_plugin(stage / "Plugins/HaivenChatStarter/HaivenChatStarter.uplugin", "5.8", "0.1.0")
            binary.require_binaries(stage, "Linux")
            first = binary.write_zip(stage, Path(directory) / "out/a.zip")
            second = binary.write_zip(stage, Path(directory) / "out/b.zip")
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with zipfile.ZipFile(first) as bundle:
                names = bundle.namelist()
                haiven = json.loads(bundle.read("HaivenChatStarter/Plugins/Haiven/Haiven.uplugin"))
                starter = json.loads(bundle.read("HaivenChatStarter/Plugins/HaivenChatStarter/HaivenChatStarter.uplugin"))
                project = json.loads(bundle.read("HaivenChatStarter/HaivenChatStarter.uproject"))
                editor = bundle.getinfo("HaivenChatStarter/Plugins/Haiven/Binaries/Linux/libUnrealEditor-Haiven.so")
            self.assertEqual(haiven["VersionName"], "0.0.0")
            self.assertEqual(haiven["EngineVersion"], "5.8.0")
            self.assertTrue(haiven["Installed"])
            self.assertEqual(starter["VersionName"], "0.1.0")
            self.assertEqual(starter["EngineVersion"], "5.8.0")
            self.assertEqual(project["EngineAssociation"], "5.8")
            self.assertIn("HaivenChatStarter/Plugins/Haiven/Intermediate/Build/Linux/UnrealEditor/Inc/Haiven/Haiven.generated.h", names)
            self.assertIn(
                "HaivenChatStarter/Plugins/Haiven/Intermediate/Build/Linux/UnrealGame/Shipping/Haiven/Haiven.precompiled",
                names,
            )
            self.assertIn(
                "HaivenChatStarter/Plugins/Haiven/Intermediate/Build/Linux/UnrealGame/Shipping/Haiven/Haiven.cpp.o",
                names,
            )
            self.assertIn("HaivenChatStarter/README.md", names)
            self.assertNotIn("HaivenChatStarter/prepare.py", names)
            self.assertNotIn(
                "HaivenChatStarter/Plugins/Haiven/Intermediate/Build/Linux/UnrealEditor/Development/Haiven/Haiven.cpp.o",
                names,
            )
            self.assertFalse(any(name.endswith(".debug") or name.endswith(".rsp") or "/Saved/" in name for name in names))
            self.assertFalse(any("libUnrealGame-" in name for name in names))
            self.assertEqual((editor.external_attr >> 16) & 0o777, 0o755)
            self.assertEqual(binary.read_descriptor(binary.HAIVEN)["VersionName"], "0.0.0")
            self.assertFalse(binary.read_descriptor(binary.STARTER)["Installed"])

    def test_stamp_refuses_a_bumped_haiven_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Haiven.uplugin"
            path.write_text(json.dumps({"VersionName": "0.1.0"}))
            with self.assertRaises(RuntimeError):
                binary.stamp_plugin(path, "5.8", "0.0.0")

    def write_stage(self, stage: Path) -> None:
        (stage / "Config").mkdir(parents=True)
        (stage / "Config/DefaultEngine.ini").write_text("ini")
        (stage / "README.md").write_text("readme")
        (stage / "prepare.py").write_text("not shipped")
        (stage / "HaivenChatStarter.uproject").write_text(json.dumps({"FileVersion": 3, "EngineAssociation": ""}))
        plugins = {
            "Haiven": "0.0.0",
            "HaivenChatStarter": "0.1.0",
        }
        for plugin, version in plugins.items():
            root = stage / "Plugins" / plugin
            root.mkdir(parents=True)
            (root / f"{plugin}.uplugin").write_text(json.dumps({"VersionName": version, "Installed": False}))
        for plugin, relative in binary.binary_names("Linux"):
            path = stage / "Plugins" / plugin / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"binary")
        for plugin, modules in binary.GAME_MODULES.items():
            for module in modules:
                for configuration in ("Development", "Shipping"):
                    self.write_manifest(stage, plugin, module, configuration)
        debug = stage / "Plugins/Haiven/Binaries/Linux/libUnrealEditor-Haiven.so.debug"
        debug.write_bytes(b"debug")
        saved = stage / "Plugins/Haiven/Saved/z.log"
        saved.parent.mkdir()
        saved.write_text("log")
        obj = stage / "Plugins/Haiven/Intermediate/Build/Linux/UnrealEditor/Development/Haiven/Haiven.cpp.o"
        obj.parent.mkdir(parents=True)
        obj.write_bytes(b"obj")
        response = stage / "Plugins/Haiven/Intermediate/Build/Linux/UnrealGame/Shipping/Haiven/Haiven.rsp"
        response.write_text("response")
        header = stage / "Plugins/Haiven/Intermediate/Build/Linux/UnrealEditor/Inc/Haiven/Haiven.generated.h"
        header.parent.mkdir(parents=True)
        header.write_text("header")

    def write_manifest(self, stage: Path, plugin: str, module: str, configuration: str) -> None:
        directory = stage / f"Plugins/{plugin}/Intermediate/Build/Linux/UnrealGame/{configuration}/{module}"
        directory.mkdir(parents=True, exist_ok=True)
        (directory / f"{module}.cpp.o").write_bytes(b"obj")
        (directory / f"{module}.precompiled").write_text(
            json.dumps({"OutputFiles": [f"{module}.cpp.o"]}),
            encoding="utf-8",
        )


class WorkflowTests(unittest.TestCase):
    def test_engine_root_requires_a_matching_build_version(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            version = root / "Engine/Build/Build.version"
            version.parent.mkdir(parents=True)
            version.write_text(json.dumps({"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 2}))
            engine = {
                "version": [5, 8, 2],
                "rootEnv": "HAIVEN_TEST_UNREAL_ROOT",
                "installPath": "/opt/unreal/does-not-exist",
            }
            os.environ["HAIVEN_TEST_UNREAL_ROOT"] = str(root)
            try:
                self.assertEqual(binary.engine_root(engine), root.resolve())
            finally:
                del os.environ["HAIVEN_TEST_UNREAL_ROOT"]

    def test_binary_workflow_does_not_cook_or_run_on_pull_requests(self):
        workflow = (binary.REPO / ".github/workflows/release-binaries.yml").read_text(encoding="utf-8")
        script = (binary.HERE / "binary_release.py").read_text(encoding="utf-8")
        for text in (workflow, script):
            self.assertNotIn("BuildCookRun", text)
            self.assertNotIn("RunUAT", text)
        self.assertIn("-plugin=", script)
        self.assertNotIn("\n  pull_request:", workflow)
        self.assertIn('tags: ["v*"]', workflow)
        self.assertIn("self-hosted", workflow)
        self.assertIn("linux", workflow)
        self.assertNotIn("windows-", workflow)
        self.assertIn("SHA256SUMS-binaries", workflow)
        self.assertNotIn("> SHA256SUMS\n", workflow)
        self.assertNotIn("SHA256SUMS --clobber", workflow)
        self.assertIn("unreal-5.8.2-clang20", (binary.HERE / "engines.json").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
