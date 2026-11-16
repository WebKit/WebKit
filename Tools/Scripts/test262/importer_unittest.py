#!/usr/bin/env python3

# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import contextlib
import io
import os
import tempfile
import unittest
from unittest import mock

import importer


class TestDefaultPaths(unittest.TestCase):
    def test_test262_dir_resolves_into_jstests(self):
        self.assertTrue(
            importer._TEST262_DIR.replace(os.sep, "/").endswith("JSTests/test262")
        )
        self.assertTrue(os.path.isdir(importer._TEST262_DIR), importer._TEST262_DIR)
        self.assertNotIn(
            os.path.join("Tools", "JSTests"), importer._TEST262_DIR
        )

    def test_revision_and_summary_live_beside_tests(self):
        parent = os.path.dirname(importer._REVISION_FILE)
        self.assertEqual(parent, importer._TEST262_DIR)
        self.assertEqual(os.path.dirname(importer._SUMMARY_FILE), importer._TEST262_DIR)


class TestReadRevision(unittest.TestCase):
    def _write(self, contents):
        f = tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False, encoding="utf-8"
        )
        f.write(contents)
        f.close()
        self.addCleanup(os.unlink, f.name)
        return f.name

    def test_parses_revision(self):
        path = self._write(
            "test262 remote url: https://github.com/tc39/test262.git\n"
            "test262 revision: 7a096c205fd422ecba49a407d5ac4d1b3f842296\n"
        )
        self.assertEqual(
            importer._read_revision(path),
            "7a096c205fd422ecba49a407d5ac4d1b3f842296",
        )

    def test_missing_file_returns_none(self):
        self.assertIsNone(
            importer._read_revision("/nonexistent/test262-Revision.txt")
        )

    def test_no_revision_line_returns_none(self):
        path = self._write("nothing useful here\n")
        self.assertIsNone(importer._read_revision(path))


class TestWriteRevision(unittest.TestCase):
    def test_exact_format(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "test262-Revision.txt")
            importer._write_revision(path, "abc123", "https://example.com/t.git")
            with open(path, "r", encoding="utf-8") as f:
                self.assertEqual(
                    f.read(),
                    "test262 remote url: https://example.com/t.git\n"
                    "test262 revision: abc123\n",
                )

    def test_round_trips_through_read(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "test262-Revision.txt")
            importer._write_revision(path, "deadbeef", "https://example.com/t.git")
            self.assertEqual(importer._read_revision(path), "deadbeef")


class TestNormalizeSummary(unittest.TestCase):
    def test_strips_prefixes_and_trailing_newline(self):
        t262 = "/webkit/JSTests/test262"
        src = "/tmp/import-src"
        raw = (
            f"A\t{src}/harness/added.js\n"
            f"D\t{t262}/harness/deleted.js\n"
            f"M\t{t262}/test/modified.js\n"
        )
        self.assertEqual(
            importer._normalize_summary(raw, t262, src),
            "A harness/added.js\n"
            "D harness/deleted.js\n"
            "M test/modified.js",
        )

    def test_no_trailing_newline_added(self):
        t262 = "/webkit/JSTests/test262"
        src = "/tmp/import-src"
        raw = f"A\t{src}/harness/x.js\n"
        result = importer._normalize_summary(raw, t262, src)
        self.assertFalse(result.endswith("\n"))
        self.assertEqual(result, "A harness/x.js")

    def test_rename_two_paths(self):
        t262 = "/webkit/JSTests/test262"
        src = "/tmp/import-src"
        raw = f"R100\t{t262}/test/old.js\t{src}/test/new.js\n"
        self.assertEqual(
            importer._normalize_summary(raw, t262, src),
            "R100 test/old.js test/new.js",
        )

    def test_paths_with_regex_metacharacters(self):
        # A '+' in the temp dir name must be treated literally, not as a regex
        # quantifier.
        t262 = "/webkit/JSTests/test262"
        src = "/tmp/a+b.c"
        raw = f"A\t{src}/harness/x.js\n"
        self.assertEqual(
            importer._normalize_summary(raw, t262, src),
            "A harness/x.js",
        )

    def test_empty_stays_empty(self):
        self.assertEqual(importer._normalize_summary("", "/a", "/b"), "")


class TestWriteSummary(unittest.TestCase):
    def test_writes_verbatim_without_trailing_newline(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "latest-changes-summary.txt")
            summary = "A harness/x.js\nM test/y.js"
            importer._write_summary(path, summary)
            with open(path, "rb") as f:
                data = f.read()
            self.assertEqual(data, summary.encode("utf-8"))
            self.assertFalse(data.endswith(b"\n"))


class TestGetSummary(unittest.TestCase):
    @mock.patch("importer.subprocess.run")
    def test_combines_harness_and_test_diffs(self, mock_run):
        t262 = "/webkit/JSTests/test262"
        src = "/tmp/src"
        mock_run.side_effect = [
            mock.Mock(stdout=f"M\t{t262}/harness/a.js\n"),
            mock.Mock(stdout=f"A\t{src}/test/b.js\n"),
        ]
        summary = importer._get_summary(t262, src)
        self.assertEqual(summary, "M harness/a.js\nA test/b.js")
        # One diff invocation per subtree.
        self.assertEqual(mock_run.call_count, 2)

    @mock.patch("importer.subprocess.run")
    def test_uses_no_index_and_diff_filter(self, mock_run):
        mock_run.return_value = mock.Mock(stdout="")
        importer._get_summary("/t262", "/src")
        cmd = mock_run.call_args_list[0][0][0]
        self.assertIn("--no-index", cmd)
        self.assertIn("--name-status", cmd)
        self.assertIn("--diff-filter=ADRM", cmd)


class TestGetNewRevision(unittest.TestCase):
    @mock.patch("importer._git")
    def test_returns_revision_and_tracking(self, mock_git):
        mock_git.side_effect = [
            "https://github.com/tc39/test262.git",  # ls-remote --get-url
            "main",                                 # rev-parse --abbrev-ref HEAD
            "abc123",                               # rev-parse HEAD
        ]
        with contextlib.redirect_stdout(io.StringIO()):
            revision, tracking = importer._get_new_revision("/src")
        self.assertEqual(revision, "abc123")
        self.assertEqual(tracking, "https://github.com/tc39/test262.git")

    @mock.patch("importer._git")
    def test_empty_revision_dies(self, mock_git):
        mock_git.side_effect = ["url", "main", ""]
        with contextlib.redirect_stdout(io.StringIO()), \
             contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                importer._get_new_revision("/src")


class TestValidateSourceDir(unittest.TestCase):
    def _make_valid_source(self, root):
        for sub in (".git", "test", "harness"):
            os.makedirs(os.path.join(root, sub))

    def test_valid_source_passes(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "src")
            self._make_valid_source(src)
            # Should not raise.
            importer._validate_source_dir(src, os.path.join(d, "current"))

    def test_missing_subdirs_dies(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "src")
            os.makedirs(src)  # no .git/test/harness
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    importer._validate_source_dir(src, os.path.join(d, "current"))

    def test_same_as_current_dies(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "src")
            self._make_valid_source(src)
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    importer._validate_source_dir(src, src)


class TestTransferFiles(unittest.TestCase):
    def test_replaces_existing_trees(self):
        with tempfile.TemporaryDirectory() as d:
            test262_dir = os.path.join(d, "test262")
            source_dir = os.path.join(d, "src")

            # Existing (stale) trees in the destination.
            os.makedirs(os.path.join(test262_dir, "harness"))
            os.makedirs(os.path.join(test262_dir, "test"))
            with open(os.path.join(test262_dir, "harness", "old.js"), "w") as f:
                f.write("old")

            # Incoming trees in the source.
            os.makedirs(os.path.join(source_dir, "harness"))
            os.makedirs(os.path.join(source_dir, "test"))
            with open(os.path.join(source_dir, "harness", "new.js"), "w") as f:
                f.write("new")

            with contextlib.redirect_stdout(io.StringIO()):
                importer._transfer_files(test262_dir, source_dir)

            # The stale file is gone, the new one is in place, and the source
            # trees have been moved out.
            self.assertFalse(
                os.path.exists(os.path.join(test262_dir, "harness", "old.js"))
            )
            self.assertTrue(
                os.path.exists(os.path.join(test262_dir, "harness", "new.js"))
            )
            self.assertFalse(os.path.exists(os.path.join(source_dir, "harness")))
            self.assertFalse(os.path.exists(os.path.join(source_dir, "test")))


class TestGit(unittest.TestCase):
    @mock.patch("importer.subprocess.run")
    def test_strips_output(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="abc123\n", stderr="")
        self.assertEqual(importer._git(["rev-parse", "HEAD"]), "abc123")

    @mock.patch("importer.subprocess.run")
    def test_nonzero_exit_dies(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=128, stdout="", stderr="boom\n")
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                importer._git(["rev-parse", "HEAD"])


class TestMainFlow(unittest.TestCase):
    def test_same_revision_skips_import(self):
        with tempfile.TemporaryDirectory() as d:
            source_dir = os.path.join(d, "src")
            for sub in (".git", "test", "harness"):
                os.makedirs(os.path.join(source_dir, sub))

            revision_file = os.path.join(d, "test262-Revision.txt")
            importer._write_revision(revision_file, "samerev", "url")

            transfer = mock.Mock()
            with mock.patch.object(importer, "_REVISION_FILE", revision_file), \
                 mock.patch.object(importer, "_TEST262_DIR", os.path.join(d, "cur")), \
                 mock.patch.object(importer, "_get_new_revision", return_value=("samerev", "url")), \
                 mock.patch.object(importer, "_git", return_value=""), \
                 mock.patch.object(importer, "_transfer_files", transfer), \
                 mock.patch("sys.argv", ["test262-import", "-s", source_dir]):
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    importer.main()

            self.assertIn("Same revision", out.getvalue())
            transfer.assert_not_called()

    def test_new_revision_transfers_and_records(self):
        with tempfile.TemporaryDirectory() as d:
            source_dir = os.path.join(d, "src")
            for sub in (".git", "test", "harness"):
                os.makedirs(os.path.join(source_dir, sub))

            test262_dir = os.path.join(d, "cur")
            os.makedirs(test262_dir)
            revision_file = os.path.join(d, "test262-Revision.txt")
            summary_file = os.path.join(d, "latest-changes-summary.txt")
            importer._write_revision(revision_file, "oldrev", "url")

            with mock.patch.object(importer, "_REVISION_FILE", revision_file), \
                 mock.patch.object(importer, "_SUMMARY_FILE", summary_file), \
                 mock.patch.object(importer, "_TEST262_DIR", test262_dir), \
                 mock.patch.object(importer, "_get_new_revision", return_value=("newrev", "https://example.com/t.git")), \
                 mock.patch.object(importer, "_git", return_value=""), \
                 mock.patch.object(importer, "_get_summary", return_value="A test/new.js"), \
                 mock.patch.object(importer, "_transfer_files") as transfer, \
                 mock.patch("sys.argv", ["test262-import", "-s", source_dir]):
                with contextlib.redirect_stdout(io.StringIO()):
                    importer.main()

            transfer.assert_called_once()
            self.assertEqual(importer._read_revision(revision_file), "newrev")
            with open(summary_file, "r", encoding="utf-8") as f:
                self.assertEqual(f.read(), "A test/new.js")

    def test_dirty_local_source_dies(self):
        with tempfile.TemporaryDirectory() as d:
            source_dir = os.path.join(d, "src")
            for sub in (".git", "test", "harness"):
                os.makedirs(os.path.join(source_dir, sub))
            revision_file = os.path.join(d, "test262-Revision.txt")
            importer._write_revision(revision_file, "oldrev", "url")

            with mock.patch.object(importer, "_REVISION_FILE", revision_file), \
                 mock.patch.object(importer, "_TEST262_DIR", os.path.join(d, "cur")), \
                 mock.patch.object(importer, "_git", return_value=" M harness/x.js"), \
                 mock.patch("sys.argv", ["test262-import", "-s", source_dir]):
                with contextlib.redirect_stdout(io.StringIO()), \
                     contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit):
                        importer.main()


if __name__ == "__main__":
    unittest.main()
