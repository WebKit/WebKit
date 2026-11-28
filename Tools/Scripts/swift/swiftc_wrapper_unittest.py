#!/usr/bin/env python3
"""Unit tests for swiftc-wrapper.py.

Run with: Tools/Scripts/test-webkitpy swift
"""

import contextlib
import importlib.util
import io
import os
import shlex
import sys
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

_WRAPPER_PATH = Path(__file__).with_name("swiftc-wrapper.py")
_SPEC = importlib.util.spec_from_file_location("swiftc_wrapper", _WRAPPER_PATH)
swiftc_wrapper = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(swiftc_wrapper)


class SwiftcWrapperTest(unittest.TestCase):
    def dry_run(self, *argv):
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = swiftc_wrapper.main(["--dry-run", *argv])
        self.assertEqual(rc, 0)
        return shlex.split(stdout.getvalue().strip())

    def test_drops_num_threads_in_batch_mode(self):
        command = self.dry_run(
            "--original-swift-compiler=/usr/bin/swiftc",
            "-j", "18",
            "-num-threads", "18",
            "-c",
            "-incremental",
            "-enable-batch-mode",
            "foo.swift",
        )
        self.assertEqual(
            command,
            ["/usr/bin/swiftc", "-j", "18", "-c", "-incremental", "-enable-batch-mode", "foo.swift"],
        )

    def test_drops_equals_spelled_num_threads_in_batch_mode(self):
        command = self.dry_run(
            "--original-swift-compiler=/usr/bin/swiftc",
            "-num-threads=18",
            "-enable-batch-mode",
            "foo.swift",
        )
        self.assertEqual(command, ["/usr/bin/swiftc", "-enable-batch-mode", "foo.swift"])

    def test_preserves_num_threads_outside_batch_mode(self):
        command = self.dry_run(
            "--original-swift-compiler=/usr/bin/swiftc",
            "-j", "18",
            "-num-threads", "18",
            "-c",
            "-wmo",
            "foo.swift",
        )
        self.assertEqual(
            command,
            ["/usr/bin/swiftc", "-j", "18", "-num-threads", "18", "-c", "-wmo", "foo.swift"],
        )


if __name__ == "__main__":
    unittest.main()
