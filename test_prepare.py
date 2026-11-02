#!/usr/bin/env python3

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("prepare", HERE / "prepare.py")
assert SPEC and SPEC.loader
prepare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prepare)


class PrepareTest(unittest.TestCase):
    def test_assembles_each_supported_engine_without_build_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for version in ("5.6.1", "5.8.2"):
                project = prepare.assemble(root / version, version)
                descriptor = json.loads((project / "HaivenChatStarter.uproject").read_text())
                self.assertEqual(descriptor["EngineAssociation"], version.rsplit(".", 1)[0])
                self.assertTrue((project / "Plugins/Haiven/Haiven.uplugin").is_file())
                self.assertTrue(
                    (project / "Plugins/HaivenChatStarter/HaivenChatStarter.uplugin").is_file()
                )
                self.assertFalse((project / "Plugins/Haiven/Binaries").exists())

    def test_refuses_unknown_engine(self) -> None:
        with tempfile.TemporaryDirectory() as temporary, self.assertRaises(SystemExit):
            prepare.assemble(Path(temporary) / "unknown", "5.7.0")


if __name__ == "__main__":
    unittest.main()
