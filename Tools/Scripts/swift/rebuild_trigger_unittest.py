#!/usr/bin/env python3
"""Unit tests for rebuild_trigger.py and depfile.py.

Run with: Tools/Scripts/test-webkitpy swift
"""

import os
import sys
import tempfile
import unittest
from pathlib import Path

# The scripts under test are run by ninja, not imported, so they import each
# other by plain name. test-webkitpy imports this file as swift.<name>, which
# puts Tools/Scripts on sys.path but not this directory.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import depfile  # noqa: E402
import rebuild_trigger  # noqa: E402


class DepfileParseTest(unittest.TestCase):
    def parse(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "deps.d"
            path.write_text(text)
            return list(depfile.parse(path))

    def test_swiftc_spelling(self):
        self.assertEqual(self.parse("/out/a.o : /src/a.swift /src/b.h\n"),
                         ["/src/a.swift", "/src/b.h"])

    def test_merged_spelling(self):
        self.assertEqual(self.parse("/out/trigger.swift: \\\n  /src/a.h \\\n  /src/b.h\n"),
                         ["/src/a.h", "/src/b.h"])

    def test_escaped_spaces(self):
        self.assertEqual(self.parse("/out/a.o: /src/two\\ words.h /src/b.h\n"),
                         ["/src/two words.h", "/src/b.h"])

    def test_windows_drive_letter_is_not_the_separator(self):
        self.assertEqual(self.parse("C:\\out\\a.o : C:\\src\\a.h\n"), ["C:\\src\\a.h"])

    def test_round_trip_through_escape(self):
        self.assertEqual(depfile.unescape(depfile.escape("/src/two words.h")),
                         "/src/two words.h")


class RebuildTriggerTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.trigger = self.root / "Module_SwiftRebuildTrigger.swift"
        self.depfile = self.root / "Module.swift-deps.d"
        self.stamp = self.root / "Module.swift-compile.stamp"
        self.resp = self.root / "Module.platform-swift-args.resp"
        self.header = self.root / "Header.h"
        self.generated = self.root / "Generated.h"
        for path in (self.trigger, self.stamp, self.resp, self.header, self.generated):
            path.write_text("")
        self.depfile.write_text(f"{self.trigger}: \\\n  {self.header} \\\n  {self.generated}\n")
        # The shape of a build: the trigger runs first, headers the module
        # imports are generated after it, then the module compiles, and the
        # depfile is written last.
        self.set_mtime(self.resp, 1000)
        self.set_mtime(self.header, 1000)
        self.set_mtime(self.trigger, 2000)
        self.set_mtime(self.generated, 2500)
        self.set_mtime(self.stamp, 3000)
        self.set_mtime(self.depfile, 3500)

    def set_mtime(self, path, when):
        os.utime(path, (when, when))

    def mtime(self, path):
        return path.stat().st_mtime

    def run_script(self, *extra):
        argv = ["--trigger", str(self.trigger), "--depfile", str(self.depfile),
                "--stamp", str(self.stamp), "--root", str(self.root), str(self.resp), *extra]
        self.assertEqual(rebuild_trigger.main(argv), 0)

    def test_newer_depfile_alone_does_not_touch(self):
        # setUp's mtimes are a settled build: the depfile and the headers the
        # module imports are all newer than the trigger, but none is newer than
        # the compile stamp.
        self.run_script()
        self.assertEqual(self.mtime(self.trigger), 2000)

    def test_dependency_newer_than_the_compile_touches(self):
        self.set_mtime(self.generated, 3500)
        self.run_script()
        self.assertGreater(self.mtime(self.trigger), 2000)

    def test_newer_resp_touches(self):
        self.set_mtime(self.resp, 4000)
        self.run_script()
        self.assertGreater(self.mtime(self.trigger), 2000)

    def test_deleted_dependency_touches(self):
        self.header.unlink()
        self.run_script()
        self.assertGreater(self.mtime(self.trigger), 2000)

    def test_dependency_with_the_same_mtime_as_the_compile_does_not_touch(self):
        self.set_mtime(self.header, 3000)
        self.run_script()
        self.assertEqual(self.mtime(self.trigger), 2000)

    def test_missing_stamp_touches(self):
        self.stamp.unlink()
        self.run_script()
        self.assertGreater(self.mtime(self.trigger), 2000)

    def test_relative_dependency_resolves_against_root(self):
        self.depfile.write_text(f"{self.trigger}: \\\n  Header.h\n")
        self.set_mtime(self.depfile, 3500)
        self.set_mtime(self.header, 4000)
        self.run_script()
        self.assertGreater(self.mtime(self.trigger), 2000)

    def test_missing_trigger_is_created(self):
        self.trigger.unlink()
        self.run_script()
        self.assertTrue(self.trigger.exists())

    def test_missing_depfile_is_not_an_error(self):
        self.depfile.unlink()
        self.run_script()
        self.assertEqual(self.mtime(self.trigger), 2000)


if __name__ == "__main__":
    unittest.main()
