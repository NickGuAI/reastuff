# SPDX-License-Identifier: MIT
"""Exercise rollback and refusal paths using disposable copies of local files."""
from pathlib import Path
import contextlib
import io
import shutil
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import patch


class Registry:
    def __init__(self):
        self.values = {"FullScreen2": 0, "WindowScalePercent": 150}

    def read(self):
        return dict(self.values)

    def set(self, key, value):
        if value is None:
            self.values.pop(key, None)
        else:
            self.values[key] = value


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="rance-installer-test-")
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name)
        source = patch.BOTTLES / "Steam" / patch.GAME_REL
        for name in {**patch.BEFORE, **patch.DEPENDENCIES}:
            target = self.game / name
            target.parent.mkdir(parents=True, exist_ok=True)
            original = source / name
            if name in patch.BEFORE and patch.digest(original) != patch.BEFORE[name]:
                original = source / patch.BACKUP / (name + ".original")
            shutil.copyfile(original, target)
        (self.game / "SaveData").mkdir()
        (self.game / "SaveData/save.asd").write_bytes(b"user progress")
        self.registry = Registry()
        stack = contextlib.ExitStack()
        self.addCleanup(stack.close)
        stack.enter_context(mock.patch.object(patch, "ensure_stopped"))
        stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def assert_original(self):
        for name, expected in patch.BEFORE.items():
            self.assertEqual(patch.digest(self.game / name), expected)
        self.assertFalse((self.game / patch.SHIM).exists())
        self.assertEqual((self.game / "SaveData/save.asd").read_bytes(), b"user progress")

    def test_roundtrip_and_idempotence(self):
        original = (self.game / "Rance7.exe").read_bytes()
        patch.install(self.game, self.registry)
        changed = (self.game / "Rance7.exe").read_bytes()
        self.assertEqual([i for i, (a, b) in enumerate(zip(original, changed)) if a != b],
                         [patch.SCALE_OFFSET])
        self.assertEqual(self.registry.values["WindowScalePercent"], 170)
        patch.install(self.game, self.registry)
        archive = patch.restore(self.game, self.registry, patch.read_manifest(self.game))
        self.assertTrue(archive.is_dir())
        self.assert_original()
        self.assertEqual(self.registry.values["WindowScalePercent"], 150)

    def test_refuse_unknown_executable(self):
        (self.game / "Rance7.exe").write_bytes(b"another game build")
        with self.assertRaisesRegex(RuntimeError, "Unsupported"):
            patch.install(self.game, self.registry)
        self.assertFalse((self.game / patch.BACKUP).exists())
        self.assertFalse((self.game / patch.SHIM).exists())

    def test_refuse_existing_untracked_shim(self):
        (self.game / patch.SHIM).write_bytes(b"user mod")
        with self.assertRaisesRegex(RuntimeError, "already exists"):
            patch.install(self.game, self.registry)
        self.assertEqual((self.game / patch.SHIM).read_bytes(), b"user mod")

    def test_refuse_overwriting_later_game_edit(self):
        patch.install(self.game, self.registry)
        (self.game / "SACT_DLL.inc").write_bytes(b"user modified load list")
        with self.assertRaisesRegex(RuntimeError, "changed after"):
            patch.restore(self.game, self.registry, patch.read_manifest(self.game))
        self.assertEqual(patch.digest(self.game / "Rance7.exe"), patch.SCALE_EXE_HASH)
        self.assertEqual((self.game / "SACT_DLL.inc").read_bytes(), b"user modified load list")

    def test_preserve_new_window_preference(self):
        patch.install(self.game, self.registry)
        self.registry.values["WindowScalePercent"] = 100
        patch.restore(self.game, self.registry, patch.read_manifest(self.game))
        self.assert_original()
        self.assertEqual(self.registry.values["WindowScalePercent"], 100)

    def test_rollback_mid_install_failure(self):
        original_write = patch.atomic_write
        failed = False

        def fail_once(path, data):
            nonlocal failed
            if path == self.game / "SACT_DLL.inc" and not failed:
                failed = True
                raise OSError("injected write failure")
            return original_write(path, data)

        with mock.patch.object(patch, "atomic_write", fail_once):
            with self.assertRaisesRegex(RuntimeError, "was rolled back"):
                patch.install(self.game, self.registry)
        self.assert_original()
        self.assertFalse((self.game / patch.BACKUP).exists())
        self.assertEqual(self.registry.values["WindowScalePercent"], 150)


if __name__ == "__main__":
    unittest.main()
