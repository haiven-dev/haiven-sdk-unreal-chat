import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile

spec = importlib.util.spec_from_file_location("haiven_chat_release", Path(__file__).with_name("release.py"))
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class VersionTests(unittest.TestCase):
    def test_tag_must_match_descriptor(self):
        with mock.patch.object(release, "descriptor_version", return_value="1.2.3"):
            self.assertEqual(release.release_version("v1.2.3"), "1.2.3")
            self.assertEqual(release.release_version(None), "1.2.3")
            for tag in ("v1.2.4", "1.2.3", "v1.2", "v1.2.3-rc.1", "vx.y.z", "v1.2.3/../x"):
                with self.subTest(tag=tag), self.assertRaises(RuntimeError):
                    release.release_version(tag)

    def test_prerelease_tag(self):
        with mock.patch.object(release, "descriptor_version", return_value="1.2.3-rc.1"):
            self.assertEqual(release.release_version("v1.2.3-rc.1"), "1.2.3-rc.1")

    def test_descriptor_must_be_semver(self):
        with mock.patch.object(release, "descriptor_version", return_value="1.0"):
            with self.assertRaises(RuntimeError):
                release.release_version(None)

    def test_checked_rejects_path_like_versions(self):
        for version in ("../1.2.3", "1.2.3/x", ""):
            with self.subTest(version=version), self.assertRaises(RuntimeError):
                release.checked(version)


class SourceZipTests(unittest.TestCase):
    def test_source_zip_excludes_build_products(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in (
                "HaivenChatStarter.uproject",
                "README.md",
                "Config/DefaultGame.ini",
                "Plugins/Haiven/Haiven.uplugin",
                "Plugins/Haiven/Binaries/Linux/lib.so",
                "Plugins/Haiven/Source/Haiven/Intermediate/y.o",
                "Plugins/HaivenChatStarter/Saved/z.log",
                "Plugins/__pycache__/x.pyc",
                "prepare.py",
                "dist/not-shipped.zip",
            ):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(relative)
            first = release.write_zip(root, root / "out/a.zip")
            second = release.write_zip(root, root / "out/b.zip")
            with zipfile.ZipFile(first) as bundle:
                self.assertEqual(
                    bundle.namelist(),
                    [
                        "HaivenChatStarter/HaivenChatStarter.uproject",
                        "HaivenChatStarter/README.md",
                        "HaivenChatStarter/Config/DefaultGame.ini",
                        "HaivenChatStarter/Plugins/Haiven/Haiven.uplugin",
                    ],
                )
            self.assertEqual(first.read_bytes(), second.read_bytes(), "zip output must be deterministic")

    def test_repository_source_bundle(self):
        self.assertEqual(release.descriptor_version(), "0.1.0")
        with tempfile.TemporaryDirectory() as directory:
            archive = release.source("0.1.0", directory)
            self.assertEqual(archive.name, "haiven-unreal-chat-source-0.1.0.zip")
            with zipfile.ZipFile(archive) as bundle:
                names = bundle.namelist()
            self.assertIn("HaivenChatStarter/HaivenChatStarter.uproject", names)
            self.assertIn("HaivenChatStarter/README.md", names)
            self.assertIn("HaivenChatStarter/Config/DefaultEngine.ini", names)
            self.assertIn("HaivenChatStarter/Plugins/Haiven/Haiven.uplugin", names)
            self.assertIn("HaivenChatStarter/Plugins/HaivenChatStarter/HaivenChatStarter.uplugin", names)
            self.assertIn(
                "HaivenChatStarter/Plugins/HaivenChatStarter/Content/Fixtures/chat-session.json",
                names,
            )
            self.assertTrue(all(name.startswith("HaivenChatStarter/") for name in names))
            self.assertFalse(any(name.endswith("prepare.py") or name.endswith("test_prepare.py") for name in names))
            self.assertFalse(any("/ci/" in f"/{name}" for name in names))
            self.assertFalse(any(part in release.EXCLUDED_DIRS for name in names for part in name.split("/")))
            self.assertFalse(
                any(name.endswith((".uasset", ".umap", ".exe", ".so", ".dll", ".pyc")) for name in names)
            )

    def test_shipped_readme_uses_the_archive_layout(self):
        readme = (release.REPO / "README.md").read_text()
        self.assertIn("EngineAssociation", readme)
        self.assertIn("Unreal Editor 5.8", readme)
        self.assertIn("HaivenChatStarter.uproject", readme)
        self.assertIn("Config/DefaultGame.ini", readme)
        self.assertIn("HAIVEN_DEMO_ACCESS_TOKEN", readme)
        self.assertIn("Haiven.ChatStarter", readme)
        for absent in (
            "prepare.py",
            "demos/unreal-chat-starter",
            "sdks/unreal",
            "engines.json",
            "unreal-editor",
            "/tmp/haiven-chat-starter",
        ):
            self.assertNotIn(absent, readme)
        with tempfile.TemporaryDirectory() as directory:
            archive = release.source("0.1.0", directory)
            with zipfile.ZipFile(archive) as bundle:
                shipped = bundle.read("HaivenChatStarter/README.md").decode()
            self.assertEqual(shipped, readme)

    def test_source_refuses_a_version_that_does_not_match(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(RuntimeError):
            release.source("9.9.9", directory)

    def test_live_descriptor_is_in_the_uproject_plugins(self):
        project = json.loads((release.REPO / "HaivenChatStarter.uproject").read_text())
        enabled = [row["Name"] for row in project["Plugins"]]
        self.assertEqual(enabled, ["Haiven", "HaivenChatStarter"])


if __name__ == "__main__":
    unittest.main()
