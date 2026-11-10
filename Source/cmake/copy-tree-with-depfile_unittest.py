#!/usr/bin/env python3
"""Unit tests for copy-tree-with-depfile.py.

Run with: python3 Source/cmake/copy-tree-with-depfile_unittest.py
"""

import contextlib
import importlib.util
import io
import os
import re
import stat
import tempfile
import time
import unittest

_script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "copy-tree-with-depfile.py")
_spec = importlib.util.spec_from_file_location("copy_tree_with_depfile", _script)
copy_tree_with_depfile = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(copy_tree_with_depfile)


def parse_depfile(path):
    with open(path) as f:
        text = f.read().replace("\\\n", " ")
    target, _, deps = text.partition(": ")
    split = re.split(r"(?<!\\) +", deps.strip())

    def unescape(token):
        return re.sub(r"\\([ #])", r"\1", token).replace("$$", "$")

    return unescape(target), [unescape(token) for token in split if token]


class CopyTreeTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = os.path.realpath(self._tmp.name)
        self.source = os.path.join(self.root, "source")
        self.dest = os.path.join(self.root, "dest")
        self.depfile = os.path.join(self.root, "out.stamp.d")
        self.stamp = os.path.join(self.root, "out.stamp")
        os.makedirs(self.source)

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, relpath, contents="contents"):
        path = os.path.join(self.source, relpath)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(contents)
        return path

    def run_copy(self, *extra):
        argv = ["--source", self.source, "--dest", self.dest,
                "--stamp", self.stamp, "--depfile", self.depfile] + list(extra)
        self.assertEqual(copy_tree_with_depfile.main(argv), 0)
        return parse_depfile(self.depfile)

    def test_copies_tree_and_lists_deps(self):
        self.write("a.txt")
        self.write("sub/b.txt")
        target, deps = self.run_copy()

        self.assertEqual(target, self.stamp)
        self.assertTrue(os.path.isfile(os.path.join(self.dest, "sub", "b.txt")))
        self.assertEqual(deps, [
            self.source,
            os.path.join(self.source, "a.txt"),
            os.path.join(self.source, "sub"),
            os.path.join(self.source, "sub", "b.txt"),
            os.path.join(self.dest, "a.txt"),
            os.path.join(self.dest, "sub", "b.txt"),
        ])

    def test_depfile_escaping(self):
        self.write("dollar$sign.txt")
        self.write("hash#mark.txt")
        self.write("two words.txt")
        self.run_copy()

        with open(self.depfile) as f:
            text = f.read()
        self.assertIn("dollar$$sign.txt", text)
        self.assertIn("hash\\#mark.txt", text)
        self.assertIn("two\\ words.txt", text)
        _, deps = parse_depfile(self.depfile)
        self.assertIn(os.path.join(self.source, "two words.txt"), deps)

    def test_symlinks_are_copied_as_links(self):
        self.write("dir/file.txt")
        os.symlink("dir/file.txt", os.path.join(self.source, "file-link"))
        os.symlink("dir", os.path.join(self.source, "dir-link"))
        os.symlink("missing", os.path.join(self.source, "dangling"))
        _, deps = self.run_copy()

        self.assertEqual(os.readlink(os.path.join(self.dest, "file-link")), "dir/file.txt")
        self.assertEqual(os.readlink(os.path.join(self.dest, "dir-link")), "dir")
        self.assertEqual(os.readlink(os.path.join(self.dest, "dangling")), "missing")
        # Links are tracked at the source but not traversed, and dangling ones
        # aren't tracked at all, since ninja would always see them as missing.
        self.assertIn(os.path.join(self.source, "dir-link"), deps)
        self.assertNotIn(os.path.join(self.source, "dir-link", "file.txt"), deps)
        self.assertNotIn(os.path.join(self.source, "dangling"), deps)
        self.assertNotIn(os.path.join(self.dest, "file-link"), deps)

    def test_exclude_matches_source_relative_paths(self):
        self.write("cocoa/top.txt")
        self.write("sub/cocoa/nested.txt")
        self.write("sub/glib/nested.txt")
        _, deps = self.run_copy("--exclude", "cocoa", "--exclude", "*/glib")

        self.assertFalse(os.path.exists(os.path.join(self.dest, "cocoa")))
        self.assertTrue(os.path.exists(os.path.join(self.dest, "sub", "cocoa", "nested.txt")))
        self.assertFalse(os.path.exists(os.path.join(self.dest, "sub", "glib")))
        self.assertNotIn(os.path.join(self.source, "cocoa"), deps)
        self.assertNotIn(os.path.join(self.source, "sub", "glib"), deps)

    def test_copies_keep_source_mtime(self):
        source_file = self.write("a.txt")
        os.utime(source_file, ns=(1_000_000_000, 1_000_000_000))
        self.run_copy()
        self.assertEqual(os.stat(os.path.join(self.dest, "a.txt")).st_mtime_ns, 1_000_000_000)

    def test_unchanged_files_are_not_rewritten(self):
        self.write("same.txt")
        changed = self.write("changed.txt", "old")
        self.run_copy()
        same_inode = os.stat(os.path.join(self.dest, "same.txt")).st_ino

        with open(changed, "w") as f:
            f.write("new contents")
        self.run_copy()

        self.assertEqual(os.stat(os.path.join(self.dest, "same.txt")).st_ino, same_inode)
        with open(os.path.join(self.dest, "changed.txt")) as f:
            self.assertEqual(f.read(), "new contents")

    def test_replaces_read_only_copies(self):
        source_file = self.write("a.txt", "old")
        os.chmod(source_file, stat.S_IRUSR)
        self.run_copy()
        os.chmod(source_file, stat.S_IRUSR | stat.S_IWUSR)
        with open(source_file, "w") as f:
            f.write("new contents")
        os.chmod(source_file, stat.S_IRUSR)
        self.run_copy()
        with open(os.path.join(self.dest, "a.txt")) as f:
            self.assertEqual(f.read(), "new contents")

    def test_stamp_is_backdated_to_start(self):
        self.write("a.txt")
        before = time.time_ns()
        self.run_copy()
        stamp_mtime = os.stat(self.stamp).st_mtime_ns
        self.assertGreaterEqual(stamp_mtime, before - 1_000_000)
        self.assertLessEqual(stamp_mtime, os.stat(self.depfile).st_mtime_ns)

    def test_delete_destination(self):
        self.write("a.txt")
        os.makedirs(self.dest)
        with open(os.path.join(self.dest, "stale.txt"), "w"):
            pass
        self.run_copy("--delete-destination")
        self.assertEqual(os.listdir(self.dest), ["a.txt"])

    def test_merges_into_existing_destination(self):
        self.write("a.txt")
        os.makedirs(self.dest)
        with open(os.path.join(self.dest, "other.txt"), "w"):
            pass
        self.run_copy()
        self.assertEqual(sorted(os.listdir(self.dest)), ["a.txt", "other.txt"])

    def test_replaces_symlink_where_directory_belongs(self):
        self.write("dir/a.txt")
        elsewhere = os.path.join(self.root, "elsewhere")
        os.makedirs(elsewhere)
        os.makedirs(self.dest)
        os.symlink(elsewhere, os.path.join(self.dest, "dir"))
        self.run_copy()
        self.assertFalse(os.path.islink(os.path.join(self.dest, "dir")))
        self.assertEqual(os.listdir(elsewhere), [])

    def test_refuses_overlapping_destinations(self):
        for dest in (self.source, os.path.join(self.source, "inner"), self.root):
            with self.assertRaises(SystemExit):
                copy_tree_with_depfile.main(["--source", self.source, "--dest", dest,
                                             "--stamp", self.stamp, "--depfile", self.depfile])

    def test_scan_only(self):
        self.write("a.txt")
        other = os.path.join(self.root, "other")
        os.makedirs(other)
        output = os.path.join(self.root, "Assets.car")
        self.assertEqual(copy_tree_with_depfile.main([
            "--source", self.source, "--source", other,
            "--depfile", self.depfile, "--depfile-target", output,
        ]), 0)

        target, deps = parse_depfile(self.depfile)
        self.assertEqual(target, output)
        self.assertEqual(deps, [self.source, os.path.join(self.source, "a.txt"), other])
        self.assertFalse(os.path.exists(self.dest))

    def test_scan_only_requires_target(self):
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            copy_tree_with_depfile.main(["--source", self.source, "--depfile", self.depfile])


if __name__ == "__main__":
    unittest.main()
