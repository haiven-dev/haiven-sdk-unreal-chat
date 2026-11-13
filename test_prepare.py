#!/usr/bin/env python3

import importlib
import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("prepare", HERE / "prepare.py")
assert SPEC and SPEC.loader
prepare = importlib.util.module_from_spec(SPEC)
sys.modules[prepare.__name__] = prepare
SPEC.loader.exec_module(prepare)

# HaivenChatStarter includes these. The pinned SDK checkout does not have them.
PLUGIN_FILES = (
    "Haiven.uplugin",
    "Source/Haiven/Public/HaivenChat.h",
    "Source/Haiven/Public/Blueprint/HaivenBlueprintTypes.h",
    "Source/Haiven/Public/Blueprint/HaivenBlueprintRealtime.h",
    "Source/Haiven/Public/Blueprint/HaivenBlueprintContracts.h",
    "Source/ThirdParty/HaivenCpp/include/HaivenFrames.h",
)


class PrepareTest(unittest.TestCase):
    def _assert_repository_plugin(self, project: Path) -> None:
        for relative in PLUGIN_FILES:
            copied = project / "Plugins/Haiven" / relative
            self.assertTrue(copied.is_file(), relative)
            self.assertEqual(copied.read_bytes(), (HERE / "Plugins/Haiven" / relative).read_bytes())

    def test_assembles_each_supported_engine_without_build_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for version in ("5.6.1", "5.8.2"):
                project = prepare.assemble(root / version, version)
                descriptor = json.loads((project / "HaivenChatStarter.uproject").read_text())
                self.assertEqual(descriptor["EngineAssociation"], version.rsplit(".", 1)[0])
                self._assert_repository_plugin(project)
                self.assertTrue(
                    (project / "Plugins/HaivenChatStarter/HaivenChatStarter.uplugin").is_file()
                )
                self.assertFalse((project / "Plugins/Haiven/Binaries").exists())
                # HaivenChatStarter includes these. The pinned SDK checkout does not have them.
                for relative in (
                    "Source/Haiven/Public/HaivenChat.h",
                    "Source/Haiven/Public/Blueprint/HaivenBlueprintTypes.h",
                    "Source/Haiven/Public/Blueprint/HaivenBlueprintRealtime.h",
                    "Source/Haiven/Public/Blueprint/HaivenBlueprintContracts.h",
                    "Source/ThirdParty/HaivenCpp/include/HaivenFrames.h",
                ):
                    copied = project / "Plugins/Haiven" / relative
                    self.assertTrue(copied.is_file(), relative)
                    self.assertEqual(
                        copied.read_bytes(), (HERE / "Plugins/Haiven" / relative).read_bytes()
                    )

    def test_keeps_repository_plugin_when_sdk_unreal_is_a_decoy(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            decoy = root / "decoy-sdk"
            decoy_plugin = decoy / "Haiven"
            (decoy_plugin / "Source/Haiven/Public").mkdir(parents=True)
            (decoy_plugin / "Haiven.uplugin").write_text('{"FriendlyName":"decoy"}\n')
            (decoy_plugin / "Source/Haiven/Public/HaivenChat.h").write_text("decoy\n")
            previous = os.environ.get("HAIVEN_SDK_UNREAL")
            os.environ["HAIVEN_SDK_UNREAL"] = str(decoy)
            try:
                importlib.reload(prepare)
                project = prepare.assemble(root / "project", "5.8.2")
            finally:
                if previous is None:
                    os.environ.pop("HAIVEN_SDK_UNREAL", None)
                else:
                    os.environ["HAIVEN_SDK_UNREAL"] = previous
                importlib.reload(prepare)
            self._assert_repository_plugin(project)

    def test_refuses_unknown_engine(self) -> None:
        with tempfile.TemporaryDirectory() as temporary, self.assertRaises(SystemExit):
            prepare.assemble(Path(temporary) / "unknown", "5.7.0")


if __name__ == "__main__":
    unittest.main()
