#!/usr/bin/env python3

# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Unit tests for update-libwebrtc.py. They run against scratch git repositories
# (with a local bare repository standing in for the WebKit fork), fake Chromium
# trees and fake build scripts, so they need neither the network nor a WebKit
# build. Run with:  python3 Tools/Scripts/test-update-libwebrtc.py [-v]

import contextlib
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parent / "update-libwebrtc.py"
LIBWEBRTC = "Source/ThirdParty/libwebrtc"
ABSEIL = f"{LIBWEBRTC}/Source/third_party/abseil-cpp"
WEBRTC = f"{LIBWEBRTC}/Source/webrtc"
LIBYUV = f"{LIBWEBRTC}/Source/third_party/libyuv"


def load_script():
    spec = importlib.util.spec_from_file_location("update_libwebrtc", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


u = load_script()

_saved_environment = {}


def setUpModule():
    # Keep the user's git configuration (commit signing, hooks, ...) out of the tests.
    for key, value in {"GIT_CONFIG_GLOBAL": os.devnull, "GIT_CONFIG_NOSYSTEM": "1",
                       "GIT_TERMINAL_PROMPT": "0"}.items():
        _saved_environment[key] = os.environ.get(key)
        os.environ[key] = value


def tearDownModule():
    for key, value in _saved_environment.items():
        if value is None:
            os.environ.pop(key, None)
        else:
            os.environ[key] = value


class TestCase(unittest.TestCase):
    """Common setup: a temporary directory, silenced output, no log file, and
    scripted answers for prompt()/confirm() (both read builtins.input)."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="update-libwebrtc-test-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.output = io.StringIO()
        redirect = contextlib.redirect_stdout(self.output)
        redirect.__enter__()
        self.addCleanup(redirect.__exit__, None, None, None)
        self.saved_log = (u._log_file, u._log_path)
        u._log_file, u._log_path = None, self.tmp / "test.log"
        self.addCleanup(lambda: setattr(u, "_log_file", self.saved_log[0]))
        self.addCleanup(lambda: setattr(u, "_log_path", self.saved_log[1]))

    def answers(self, *lines):
        """Script the answers to the next prompts; running out of answers fails the test."""
        patcher = mock.patch("builtins.input", side_effect=list(lines))
        self.input = patcher.start()
        self.addCleanup(patcher.stop)
        return self.input

    def no_questions(self):
        patcher = mock.patch("builtins.input", side_effect=AssertionError("unexpected question"))
        patcher.start()
        self.addCleanup(patcher.stop)


class GitTestCase(TestCase):
    def setUp(self):
        super().setUp()
        self.root = self.tmp / "wk"
        self.root.mkdir()
        self.git("init", "-q", "-b", "main")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "user.name", "Tester")
        self.git("config", "user.email", "tester@example.com")

    def git(self, *args, cwd=None, check=True):
        result = subprocess.run(["git", *args], cwd=cwd or self.root, capture_output=True, text=True)
        if check and result.returncode != 0:
            raise AssertionError(f"git {' '.join(args)} failed: {result.stderr}")
        return result.stdout.strip()

    def write(self, path, content):
        full = self.root / path
        full.parent.mkdir(parents=True, exist_ok=True)
        full.write_text(content)

    def read(self, path):
        return (self.root / path).read_text()

    def commit(self, message, files=None, author=None):
        for path, content in (files or {}).items():
            self.write(path, content)
        self.git("add", "-A")
        args = ["commit", "-q", "--allow-empty", "-m", message]
        if author:
            args += ["--author", author]
        self.git(*args)
        return self.git("rev-parse", "HEAD")

    def make_bare_remote(self, name="fork"):
        bare = self.tmp / f"{name}.git"
        self.git("init", "-q", "--bare", str(bare))
        self.git("remote", "add", name, str(bare))
        return bare

    def subjects(self, revision="HEAD", count=20):
        return self.git("log", f"-{count}", "--format=%s", revision).splitlines()


class SmallHelperTests(TestCase):
    def test_chrome_version_labels(self):
        import re
        for valid in ("m155", "m155-2", "m155-prev", "m99"):
            self.assertTrue(re.fullmatch(u.CHROME_VERSION_RE, valid), valid)
        for invalid in ("155", "m155-", "mabc", "M155", "m155 2", ""):
            self.assertFalse(re.fullmatch(u.CHROME_VERSION_RE, invalid), invalid)

    def test_sanitize_branch_component(self):
        self.assertEqual(u.sanitize_branch_component(" m155 2 "), "m155-2")
        self.assertEqual(u.sanitize_branch_component("m155/x:y"), "m155-x-y")

    def test_resync_subject_matching(self):
        for subject in ("Resync libwebrtc up to M150", "update libyuv to M143", "Update opus"):
            self.assertTrue(u.RESYNC_SUBJECT_RE.match(subject), subject)
        for subject in ("Fix resync bug", "Updated things", "Reapply WebKit-side changes"):
            self.assertFalse(u.RESYNC_SUBJECT_RE.match(subject), subject)

    def test_library_table_is_consistent(self):
        keys = [lib.key for lib in u.LIBRARIES]
        self.assertEqual(len(keys), len(set(keys)))
        self.assertNotIn("json", keys)
        self.assertIn("webrtc", keys)
        for lib in u.LIBRARIES:
            self.assertTrue(lib.dest.startswith("Source/"), lib.key)
            self.assertTrue(lib.chromium_subpath.startswith("third_party/"), lib.key)

    def test_dav1d_protects_webkit_only_files(self):
        dav1d = next(lib for lib in u.LIBRARIES if lib.key == "dav1d")
        for protected in ("/dav1d.xcodeproj", "/Configurations", "/config.h", "/config", "/vcs_version.h"):
            self.assertIn(protected, dav1d.excludes)
        self.assertIn(("third_party/dav1d/config", "config"), dav1d.extra_copies)
        self.assertIn(("third_party/dav1d/version/vcs_version.h", "vcs_version.h"), dav1d.extra_copies)

    def test_webrtc_keeps_webkit_sdk(self):
        self.assertIn("/webkit_sdk", u.WEBRTC.excludes)

    def test_select_commits_to_reapply(self):
        commits = [("aaa111", "first"), ("bbb222", "second"), ("ccc333", "third")]
        self.answers("")
        self.assertEqual(u.select_commits_to_reapply(commits), commits)
        self.answers("n")
        self.assertEqual(u.select_commits_to_reapply(commits), [])
        self.answers("3,1,bogus,9")
        self.assertEqual(u.select_commits_to_reapply(commits), [commits[0], commits[2]])

    def test_prompt_and_confirm_defaults(self):
        self.answers("", "", "y", "n")
        self.assertEqual(u.prompt("q", default="d"), "d")
        self.assertTrue(u.confirm("q", default=True))
        self.assertTrue(u.confirm("q", default=False))
        self.assertFalse(u.confirm("q", default=True))

    def test_state_round_trip(self):
        repo = GitTestCase.__new__(GitTestCase)  # only need a git dir
        root = self.tmp / "state-repo"
        root.mkdir()
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        self.assertEqual(u.load_state(root), {})
        u.save_state(root, chrome_version="m155")
        u.save_state(root, webkit_remote="fork")
        self.assertEqual(u.load_state(root), {"chrome_version": "m155", "webkit_remote": "fork"})
        self.assertTrue(u.state_path(root).is_relative_to(root))


class CMakeFileCheckTests(TestCase):
    def check(self, cmake_text, files=(), directories=()):
        root = self.tmp / "cmake-root"
        root.mkdir(exist_ok=True)
        for name in files:
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).write_text("")
        for name in directories:
            (root / name).mkdir(parents=True, exist_ok=True)
        cmake = root / "CMakeLists.txt"
        cmake.write_text(cmake_text)
        return u.find_missing_cmake_paths(cmake)

    def test_reports_missing_files_with_line_numbers(self):
        checked, skipped, missing = self.check(
            "add_library(x\n    Source/a/real.cc\n    Source/a/gone.cc\n)\n", files=["Source/a/real.cc"])
        self.assertEqual(checked, 2)
        self.assertEqual(missing, {"Source/a/gone.cc": [3]})

    def test_expands_set_variables_and_source_dir(self):
        _, _, missing = self.check(
            "set(P Source/a)\nadd_library(x ${P}/gone.cc ${CMAKE_CURRENT_SOURCE_DIR}/Source/a/gone2.cc)\n",
            directories=["Source/a"])
        self.assertEqual(set(missing), {"Source/a/gone.cc", "Source/a/gone2.cc"})

    def test_ignores_comments_unresolved_variables_and_other_paths(self):
        checked, skipped, missing = self.check(
            "# Source/a/commented.cc\nadd_library(x ${UNKNOWN}/Source/x.cc other/file.cc $<TARGET_OBJECTS:y>)\n")
        self.assertEqual((checked, missing), (0, {}))
        self.assertEqual(skipped, 1)

    def test_quoted_and_semicolon_separated_paths(self):
        _, _, missing = self.check('set(L "Source/a/x.cc;Source/a/y.cc")\nfoo("Source/a/z.cc")\n',
                                   files=["Source/a/x.cc"])
        self.assertEqual(set(missing), {"Source/a/y.cc", "Source/a/z.cc"})

    def test_directories_count_and_matching_is_case_sensitive(self):
        _, _, missing = self.check("include_directories(Source/Dir Source/dir)\n", directories=["Source/Dir"])
        self.assertEqual(set(missing), {"Source/dir"})

    def test_check_cmake_files_against_the_real_tree(self):
        # Not a check of the tree's state (that is validate's job): only that the real
        # file parses and yields plenty of references.
        real = Path(__file__).resolve().parents[2] / u.CMAKE_FILE
        if not real.exists():
            self.skipTest("not inside a WebKit checkout")
        checked, _, _ = u.find_missing_cmake_paths(real)
        self.assertGreater(checked, 1000)


class ProgressAndOutputTests(TestCase):
    def test_progress_prints_a_line_a_line_per_ten_percent_step_when_not_a_terminal(self):
        display = u.ProgressDisplay("Building")
        self.assertFalse(display.interactive)
        before = self.output.getvalue()
        for i in range(1, 101):
            display.update(f"[{i}/100] Building file{i}\n")
        lines = self.output.getvalue()[len(before):].splitlines()
        self.assertEqual(len(lines), 11)   # when crossing 0-9%, 10-19%, ..., and at 100%
        self.assertEqual(lines[0].strip(), "Building: [1/100] 1%")
        self.assertEqual(lines[-1].strip(), "Building: [100/100] 100%")

    def test_progress_falls_back_to_dots(self):
        display = u.ProgressDisplay("Building")
        before = self.output.getvalue()
        for _ in range(u.DOT_EVERY_N_LINES * 3):
            display.update("some xcodebuild output\n")
        display.finish()
        self.assertEqual(self.output.getvalue()[len(before):], "...\n")

    def test_run_with_progress_keeps_only_a_bounded_summary(self):
        script = ('for i in $(seq 1 5000); do echo "[$i/5000] Building f$i"; '
                  'if [ $((i % 100)) = 0 ]; then echo "f.cc:1: error: boom $i"; fi; done; exit 3')
        log = io.StringIO()
        status, tail, errors = u.run_with_progress(["sh", "-c", script], log, "Building")
        self.assertEqual(status, 3)
        self.assertLessEqual(len(tail), u.TAIL_LINES_ON_FAILURE)
        self.assertEqual(len(errors), u.ERROR_LINES_ON_FAILURE)
        self.assertEqual(errors[0], "f.cc:1: error: boom 100")
        self.assertIn("[5000/5000] Building f5000", tail[-2])
        self.assertEqual(tail[-1], "f.cc:1: error: boom 5000")
        self.assertIn("[exit status 3]", log.getvalue())
        self.assertGreater(log.getvalue().count("\n"), 5000)  # the full output is in the log

    def test_report_output(self):
        tail = [f"line {i}" for i in range(40)]
        before = self.output.getvalue()
        u.report_output(tail, ["x.cc: error: bad"], success=True)
        shown = self.output.getvalue()[len(before):]
        self.assertEqual(shown.count("line "), u.TAIL_LINES_ON_SUCCESS)
        self.assertNotIn("error: bad", shown)
        before = self.output.getvalue()
        u.report_output(tail, ["x.cc: error: bad"], success=False)
        shown = self.output.getvalue()[len(before):]
        self.assertEqual(shown.count("line "), u.TAIL_LINES_ON_FAILURE)
        self.assertIn("error: bad", shown)

    def test_run_logs_and_raises(self):
        with self.assertRaises(subprocess.CalledProcessError):
            u.run(["sh", "-c", "echo oops; exit 2"])
        self.assertIn("oops", self.output.getvalue())
        result = u.run(["sh", "-c", "exit 2"], check=False)
        self.assertEqual(result.returncode, 2)


@unittest.skipUnless(shutil.which("rsync"), "rsync not available")
class CopyTests(TestCase):
    def test_copy_library_mirrors_protects_and_adds_extras(self):
        chromium = self.tmp / "chromium"
        wk = self.tmp / "wk"
        (chromium / "src").mkdir(parents=True)
        (chromium / "src/new.c").write_text("new")
        (chromium / "src/sub").mkdir()
        (chromium / "src/sub/x.c").write_text("x")
        (chromium / "extra/dir").mkdir(parents=True)
        (chromium / "extra/dir/c.h").write_text("cfg")
        (chromium / "extra/f.h").write_text("version")
        (wk / "dst").mkdir(parents=True)
        (wk / "dst/stale.c").write_text("stale")
        (wk / "dst/keep.txt").write_text("webkit-only")
        (wk / "dst/sub2").mkdir()
        (wk / "dst/sub2/old.c").write_text("old")
        lib = u.Library(key="t", description="test", dest="dst", chromium_subpath="src",
                        excludes=("/keep.txt", "/dir", "/f.h"),
                        extra_copies=(("extra/dir", "dir"), ("extra/f.h", "f.h")))

        u.copy_library(chromium, wk, lib, dry_run=True)
        self.assertTrue((wk / "dst/stale.c").exists())   # a dry run changes nothing
        self.assertFalse((wk / "dst/new.c").exists())

        u.copy_library(chromium, wk, lib, dry_run=False)
        self.assertEqual((wk / "dst/new.c").read_text(), "new")
        self.assertEqual((wk / "dst/sub/x.c").read_text(), "x")
        self.assertFalse((wk / "dst/stale.c").exists())  # --delete
        self.assertFalse((wk / "dst/sub2").exists())
        self.assertEqual((wk / "dst/keep.txt").read_text(), "webkit-only")  # protected
        self.assertEqual((wk / "dst/dir/c.h").read_text(), "cfg")           # extra directory
        self.assertEqual((wk / "dst/f.h").read_text(), "version")           # extra single file

    def test_excluded_paths_survive_even_if_upstream_has_them(self):
        chromium, wk = self.tmp / "c", self.tmp / "w"
        (chromium / "src").mkdir(parents=True)
        (chromium / "src/config.h").write_text("upstream")
        (wk / "dst").mkdir(parents=True)
        (wk / "dst/config.h").write_text("webkit")
        lib = u.Library(key="t", description="t", dest="dst", chromium_subpath="src", excludes=("/config.h",))
        u.copy_library(chromium, wk, lib, dry_run=False)
        self.assertEqual((wk / "dst/config.h").read_text(), "webkit")

    def test_apply_renames(self):
        (self.tmp / "a").mkdir()
        (self.tmp / "a/x.inc").write_text("1")
        u.apply_renames(self.tmp / "a", (("x.inc", "x.inc.h"), ("missing", "other")))
        self.assertTrue((self.tmp / "a/x.inc.h").exists())
        self.assertFalse((self.tmp / "a/x.inc").exists())


class GitHelperTests(GitTestCase):
    def test_find_last_resync_commit(self):
        self.commit("Initial", {"d/a.txt": "1"})
        resync = self.commit("Resync d up to M100", {"d/a.txt": "2"})
        self.commit("Fix d", {"d/a.txt": "3"})
        self.commit("Unrelated\n\nUpdate: this body line starts like a resync", {"d/a.txt": "4"})
        self.commit("Resync other", {"other/x": "1"})
        self.assertEqual(u.find_last_resync_commit(self.root, "d"), (resync, "Resync d up to M100"))
        self.assertEqual(u.find_last_resync_commit(self.root, "nothing-here"), (None, None))

    def test_commits_to_reapply_filters_by_path_subject_and_merges(self):
        base = self.commit("Resync d", {"d/a.txt": "1", "e/b.txt": "1"})
        fix = self.commit("Fix in d", {"d/a.txt": "2", "e/b.txt": "2"})
        self.commit("Only e", {"e/b.txt": "3"})
        self.commit("Update d to M101", {"d/a.txt": "3"})
        self.git("checkout", "-q", "-b", "side", base)
        self.commit("Side change to d", {"d/side.txt": "s"})
        self.git("checkout", "-q", "main")
        self.git("merge", "-q", "--no-ff", "-m", "Merge side", "side")
        candidates, skipped = u.commits_to_reapply(self.root, f"{base}..HEAD", ["d"])
        self.assertEqual([subject for _, subject in candidates], ["Fix in d", "Side change to d"])
        self.assertEqual(candidates[0][0], fix)
        self.assertEqual([subject for _, subject in skipped], ["Update d to M101"])

    def test_detect_branch_base(self):
        base = self.commit("base", {"f": "1"})
        self.git("checkout", "-q", "-b", "feature")
        self.commit("feature", {"f": "2"})
        self.git("checkout", "-q", "main")
        self.commit("main moves", {"g": "1"})
        self.assertEqual(u.detect_branch_base(self.root, "feature"), base)
        self.git("checkout", "-q", "--orphan", "unrelated")
        self.commit("orphan", {"h": "1"})
        self.git("checkout", "-q", "main")
        self.assertIsNone(u.detect_branch_base(self.root, "unrelated"))

    def test_has_changes_and_commit_path(self):
        self.commit("init", {"d/a": "1", "e/b": "1"})
        self.assertFalse(u.has_changes(self.root, "d"))
        self.write("d/a", "2")
        self.assertTrue(u.has_changes(self.root, "d"))
        self.assertFalse(u.has_changes(self.root, "e"))
        self.assertTrue(u.commit_path(self.root, "d", "Change d"))
        self.assertFalse(u.commit_path(self.root, "d", "Nothing to commit"))
        self.assertEqual(self.subjects(count=1), ["Change d"])

    def test_has_unresolved_operation_and_checkout_guard(self):
        self.commit("init", {"f": "1"})
        self.git("checkout", "-q", "-b", "other")
        self.commit("other", {"f": "2"})
        self.git("checkout", "-q", "main")
        self.commit("main", {"f": "3"})
        self.git("cherry-pick", "other", check=False)  # conflicts
        self.assertTrue(u.has_unresolved_operation(self.root))
        self.assertFalse(u.checkout_branch_if_safe(self.root, "other"))
        self.assertEqual(u.current_branch(self.root), "main")
        self.git("cherry-pick", "--abort")
        self.assertFalse(u.has_unresolved_operation(self.root))


class CherryPickTests(GitTestCase):
    def setUp(self):
        super().setUp()
        self.commit("base", {"d/a.txt": "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\n", "e/b.txt": "b1\n"})
        self.git("checkout", "-q", "-b", "source")
        self.first = self.commit("Change d and e", {"d/a.txt": "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8 SRC\n", "e/b.txt": "b2\n"},
                                 author="Bob <bob@example.com>")
        self.git("checkout", "-q", "main")

    def test_only_the_destination_part_is_applied_and_metadata_kept(self):
        self.assertTrue(u.apply_commit_to_dests(self.root, self.first, ["d"]))
        self.assertTrue(u.commit_picked(self.root, self.first, ["d"]))
        self.assertIn("SRC", self.read("d/a.txt"))
        self.assertEqual(self.read("e/b.txt"), "b1\n")
        self.assertEqual(self.git("log", "-1", "--format=%an <%ae>"), "Bob <bob@example.com>")
        message = self.git("log", "-1", "--format=%B")
        self.assertTrue(message.startswith("Change d and e"))
        self.assertIn(f"(cherry picked from commit {self.first})", message)
        self.assertEqual(self.git("show", "--stat", "--format=", "HEAD").count("|"), 1)

    def test_commit_picked_with_nothing_staged(self):
        self.assertFalse(u.commit_picked(self.root, self.first, ["d"]))

    def test_conflict_is_reported_and_can_be_discarded(self):
        self.commit("Conflicting change on main", {"d/a.txt": "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8 MAIN\n"})
        self.assertFalse(u.apply_commit_to_dests(self.root, self.first, ["d"]))
        self.assertEqual(u.conflicted_paths(self.root), ["d/a.txt"])
        u.discard_pick_attempt(self.root, self.first, ["d"])
        self.assertEqual(u.conflicted_paths(self.root), [])
        self.assertIn("MAIN", self.read("d/a.txt"))
        self.assertEqual(self.git("status", "--porcelain", "--untracked-files=no"), "")

    def test_discard_removes_files_the_commit_added(self):
        self.git("checkout", "-q", "source")
        added = self.commit("Add a file", {"d/new.txt": "new\n"})
        self.git("checkout", "-q", "main")
        self.assertTrue(u.apply_commit_to_dests(self.root, added, ["d"]))
        self.assertTrue((self.root / "d/new.txt").exists())
        u.discard_pick_attempt(self.root, added, ["d"])
        self.assertFalse((self.root / "d/new.txt").exists())

    def test_interactive_pick_applies_all_commits_in_order(self):
        self.git("checkout", "-q", "source")
        second = self.commit("Second", {"d/c.txt": "c\n"})
        self.git("checkout", "-q", "main")
        self.no_questions()
        self.assertTrue(u.run_cherry_pick_interactively(self.root, [self.first, second], ["d"]))
        self.assertEqual(self.subjects(count=2), ["Second", "Change d and e"])

    def _conflicting_pair(self):
        self.git("checkout", "-q", "source")
        second = self.commit("Second", {"d/c.txt": "c\n"})
        self.git("checkout", "-q", "main")
        self.commit("Conflicting change on main", {"d/a.txt": "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8 MAIN\n"})
        return second

    def test_interactive_pick_skip_drops_the_conflicting_commit(self):
        second = self._conflicting_pair()
        self.answers("skip")
        self.assertTrue(u.run_cherry_pick_interactively(self.root, [self.first, second], ["d"]))
        self.assertEqual(self.subjects(count=2), ["Second", "Conflicting change on main"])
        self.assertIn("MAIN", self.read("d/a.txt"))

    def test_interactive_pick_abort_stops(self):
        second = self._conflicting_pair()
        self.answers("abort")
        self.assertFalse(u.run_cherry_pick_interactively(self.root, [self.first, second], ["d"]))
        self.assertEqual(self.subjects(count=1), ["Conflicting change on main"])
        self.assertFalse((self.root / "d/c.txt").exists())

    def test_interactive_pick_continue_after_the_user_resolves(self):
        second = self._conflicting_pair()

        def resolve(message, default=None):
            self.write("d/a.txt", "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8 RESOLVED\n")
            self.git("add", "d/a.txt")
            return "continue"

        with mock.patch.object(u, "prompt", side_effect=resolve):
            self.assertTrue(u.run_cherry_pick_interactively(self.root, [self.first, second], ["d"]))
        self.assertEqual(self.subjects(count=2), ["Second", "Change d and e"])
        self.assertIn("RESOLVED", self.read("d/a.txt"))

    def test_continue_with_unresolved_paths_asks_again(self):
        self._conflicting_pair()
        self.answers("continue", "skip")
        self.assertTrue(u.run_cherry_pick_interactively(self.root, [self.first], ["d"]))
        self.assertIn("still unresolved", self.output.getvalue())


class BranchAndRemoteTests(GitTestCase):
    def setUp(self):
        super().setUp()
        self.commit("init", {"f": "1"})
        self.bare = self.make_bare_remote()

    def test_ensure_branch_creates_checks_out_and_declines(self):
        self.answers("")                       # create? -> yes
        self.assertEqual(u.ensure_branch(self.root, "feature"), "feature")
        self.assertEqual(u.current_branch(self.root), "feature")
        self.git("checkout", "-q", "main")
        self.no_questions()
        self.assertEqual(u.ensure_branch(self.root, "feature", assume_yes=True), "feature")
        self.assertEqual(u.current_branch(self.root), "feature")
        self.git("checkout", "-q", "main")
        self.answers("n")                      # existing: check it out? -> no
        self.assertIsNone(u.ensure_branch(self.root, "feature"))
        self.assertEqual(u.current_branch(self.root), "main")

    def test_choose_branch_name_free(self):
        self.no_questions()
        self.assertEqual(u.choose_branch_name(self.root, "m155", "fork"), "all-webrtc-m155")

    def test_choose_branch_name_when_already_on_it(self):
        self.git("checkout", "-q", "-b", "all-webrtc-m155")
        self.no_questions()
        self.assertEqual(u.choose_branch_name(self.root, "m155", "fork"), "all-webrtc-m155")

    def test_choose_branch_name_taken_locally_proposes_next_free(self):
        self.git("branch", "all-webrtc-m155")
        self.answers("")
        self.assertEqual(u.choose_branch_name(self.root, "m155", "fork"), "all-webrtc-m155-2")
        self.git("branch", "all-webrtc-m155-2")
        self.answers("")
        self.assertEqual(u.choose_branch_name(self.root, "m155", "fork"), "all-webrtc-m155-3")

    def test_choose_branch_name_can_keep_the_existing_one(self):
        self.git("branch", "all-webrtc-m155")
        self.answers("e")
        self.assertEqual(u.choose_branch_name(self.root, "m155", "fork"), "all-webrtc-m155")

    def test_choose_branch_name_taken_only_on_the_remote(self):
        self.git("push", "-q", "fork", "main:all-webrtc-m155")
        self.answers("")
        self.assertEqual(u.choose_branch_name(self.root, "m155", "fork"), "all-webrtc-m155-2")
        self.assertIn("on remote 'fork'", self.output.getvalue())

    def test_label_with_suffix_is_used_as_is(self):
        self.no_questions()
        self.assertEqual(u.choose_branch_name(self.root, "m155-2", "fork"), "all-webrtc-m155-2")

    def test_remote_has_branch(self):
        self.assertFalse(u.remote_has_branch(self.root, "fork", "nope"))
        self.git("push", "-q", "fork", "main:there")
        self.assertTrue(u.remote_has_branch(self.root, "fork", "there"))

    def test_push_branch_new_existing_declined_and_forced(self):
        self.git("checkout", "-q", "-b", "all-webrtc-m155")
        self.commit("work", {"g": "1"})
        self.no_questions()
        self.assertTrue(u.push_branch(self.root, "all-webrtc-m155", "fork"))
        self.assertEqual(self.git("rev-parse", "all-webrtc-m155", cwd=self.bare),
                         self.git("rev-parse", "HEAD"))
        self.git("commit", "-q", "--amend", "-m", "work (amended)")
        pushed_before = self.git("rev-parse", "all-webrtc-m155", cwd=self.bare)
        self.answers("n")
        self.assertFalse(u.push_branch(self.root, "all-webrtc-m155", "fork"))
        self.assertEqual(self.git("rev-parse", "all-webrtc-m155", cwd=self.bare), pushed_before)
        self.answers("y")
        self.assertTrue(u.push_branch(self.root, "all-webrtc-m155", "fork"))
        self.assertEqual(self.git("rev-parse", "all-webrtc-m155", cwd=self.bare), self.git("rev-parse", "HEAD"))

    def test_resolve_webkit_remote(self):
        options = types.SimpleNamespace(webkit_remote=None)
        self.answers("fork")
        self.assertEqual(u.resolve_webkit_remote(self.root, options, {}), "fork")
        self.assertEqual(u.load_state(self.root)["webkit_remote"], "fork")
        self.assertIn(str(self.bare), self.output.getvalue())      # shows the remote's URL
        self.answers("")                                           # Enter: the remembered one
        self.assertEqual(u.resolve_webkit_remote(self.root, options, u.load_state(self.root)), "fork")
        self.no_questions()                                        # the flag skips the question
        self.assertEqual(u.resolve_webkit_remote(self.root, types.SimpleNamespace(webkit_remote="fork"), {}), "fork")
        self.assertIsNone(u.resolve_webkit_remote(self.root, types.SimpleNamespace(webkit_remote="nope"), {}))
        self.assertIn("No git remote named 'nope'", self.output.getvalue())
        self.answers("")
        self.assertIsNone(u.resolve_webkit_remote(self.root, options, {}))
        self.assertIn("A git remote name is required", self.output.getvalue())


class ReapplyFromBranchTests(GitTestCase):
    def test_reapply_from_remote_branch_limited_to_the_destinations(self):
        text = "".join(f"l{i}\n" for i in range(1, 13))
        self.commit("Resync d", {"d/a.txt": text, "e/b.txt": "1\n", "other/c.txt": "1\n"})
        bare = self.make_bare_remote()
        self.git("checkout", "-q", "-b", "tmp")
        self.commit("Resync d from somewhere", {"d/a.txt": text.replace("l1\n", "l1 RESYNCED\n")})   # excluded
        self.commit("WebKit patch", {"d/a.txt": text.replace("l12\n", "l12 patched\n"), "other/c.txt": "2\n"})
        self.git("push", "-q", "fork", "tmp:all-webrtc-m152")
        self.git("checkout", "-q", "main")
        self.git("branch", "-q", "-D", "tmp")
        self.answers("", "")   # base: the detected one; commits: all
        self.assertTrue(u.reapply_from_branch(self.root, "fork", "all-webrtc-m152", "all-webrtc", ["d"]))
        self.assertEqual(self.subjects(count=1), ["WebKit patch"])
        self.assertIn("l12 patched", self.read("d/a.txt"))
        self.assertNotIn("RESYNCED", self.read("d/a.txt"))                 # the earlier resync is not reapplied
        self.assertEqual(self.read("other/c.txt"), "1\n")                   # outside the destinations
        self.assertEqual(self.git("for-each-ref", "refs/tmp"), "")          # temporary refs cleaned up

    def test_reapply_reports_a_failed_fetch(self):
        self.commit("init", {"f": "1"})
        self.make_bare_remote()
        self.assertFalse(u.reapply_from_branch(self.root, "fork", "no-such-branch", "all-webrtc", ["f"]))


class WebKitBuildRemovalTests(GitTestCase):
    def test_reports_files_that_lost_occurrences(self):
        guarded = f"{LIBWEBRTC}/Source/webrtc/a.cc"
        other = f"{LIBWEBRTC}/Source/webrtc/b.cc"
        gone = f"{LIBWEBRTC}/Source/webrtc/c.cc"
        self.commit("base", {guarded: "#if WEBRTC_WEBKIT_BUILD\nx\n#endif\n// WEBRTC_WEBKIT_BUILD\n",
                             other: "// WEBRTC_WEBKIT_BUILD\n", gone: "// WEBRTC_WEBKIT_BUILD\n"})
        self.git("checkout", "-q", "-b", "all-webrtc-m155")
        self.assertEqual(u.report_webkit_build_removals(self.root, "main"), [])
        self.write(guarded, "#if 0\nx\n#endif\n// WEBRTC_WEBKIT_BUILD\n")   # one occurrence left
        self.write(other, "nothing\n")                                     # none left
        (self.root / gone).unlink()                                        # file removed
        findings = u.report_webkit_build_removals(self.root, "main")
        self.assertEqual({(status, path) for status, _, _, path in findings},
                         {("REDUCED", guarded), ("STRIPPED", other), ("REMOVED", gone)})
        self.assertEqual([(b, a) for s, b, a, p in findings if p == guarded], [(2, 1)])

    def test_new_occurrences_are_not_reported(self):
        path = f"{LIBWEBRTC}/Source/webrtc/a.cc"
        self.commit("base", {path: "// WEBRTC_WEBKIT_BUILD\n"})
        self.git("checkout", "-q", "-b", "feature")
        self.write(path, "// WEBRTC_WEBKIT_BUILD\n// WEBRTC_WEBKIT_BUILD\n")
        self.assertEqual(u.report_webkit_build_removals(self.root, "main"), [])

    def test_skips_when_the_main_branch_is_unknown(self):
        self.commit("base", {"f": "1"})
        self.assertEqual(u.report_webkit_build_removals(self.root, "no-such-branch"), [])
        self.assertIn("Could not find the fork point", self.output.getvalue())


class ValidationTests(GitTestCase):
    def setUp(self):
        super().setUp()
        self.commit("init", {"f": "1"})
        self.bare = self.make_bare_remote()
        self.git("checkout", "-q", "-b", "all-webrtc-m155")
        self.options = types.SimpleNamespace(no_push=False, webkit_remote="fork", release=False)

    def validate(self, results, remote="fork", branch=None):
        with mock.patch.object(u, "run_validation", side_effect=results) as run_validation, \
             mock.patch.object(u, "push_branch", return_value=True) as push_branch:
            status = u.validate_and_push(self.root, "m155", self.options, remote, branch)
        return status, run_validation, push_branch

    def test_passing_validation_pushes_the_branch(self):
        self.no_questions()
        status, run_validation, push_branch = self.validate([True])
        self.assertEqual(status, 0)
        push_branch.assert_called_once_with(self.root, "all-webrtc-m155", "fork")

    def test_retry_until_it_passes(self):
        self.answers("r", "")   # explicit retry, then Enter (retry is the default)
        status, run_validation, push_branch = self.validate([False, False, True])
        self.assertEqual((status, run_validation.call_count), (0, 3))
        push_branch.assert_called_once()

    def test_skip_does_not_push(self):
        self.answers("s")
        status, _, push_branch = self.validate([False])
        self.assertEqual(status, 1)
        push_branch.assert_not_called()

    def test_ok_pushes_despite_the_failure(self):
        self.answers("o")
        status, _, push_branch = self.validate([False])
        self.assertEqual(status, 0)
        push_branch.assert_called_once()

    def test_no_push_option(self):
        self.options.no_push = True
        self.no_questions()
        status, _, push_branch = self.validate([True])
        self.assertEqual(status, 0)
        push_branch.assert_not_called()

    def test_other_branch_is_validated_but_never_switched_or_pushed(self):
        self.git("checkout", "-q", "main")
        self.no_questions()
        status, run_validation, push_branch = self.validate([True])
        self.assertEqual(status, 0)
        self.assertEqual(u.current_branch(self.root), "main")
        push_branch.assert_not_called()
        run_validation.assert_called_once()
        self.assertIn("not 'all-webrtc-m155'", self.output.getvalue())

    def test_explicit_branch_name_overrides_the_version(self):
        self.git("checkout", "-q", "-b", "all-webrtc-m155-2")
        self.no_questions()
        status, _, push_branch = self.validate([True], branch="all-webrtc-m155-2")
        push_branch.assert_called_once_with(self.root, "all-webrtc-m155-2", "fork")

    def test_remote_is_asked_only_when_pushing(self):
        self.answers("fork")
        status, _, push_branch = self.validate([True], remote=None)
        push_branch.assert_called_once_with(self.root, "all-webrtc-m155", "fork")

    def _run_validation(self, cmake_ok=True, removals=(), answers=(), build_ok=True):
        options = types.SimpleNamespace(main_branch="main", release=False)
        self.answers(*answers)
        with mock.patch.object(u, "check_cmake_files", return_value=cmake_ok), \
             mock.patch.object(u, "report_webkit_build_removals", side_effect=list(removals)) as report, \
             mock.patch.object(u, "build_and_test", return_value=build_ok) as build:
            return u.run_validation(self.root, options, self.tmp / "build.log"), report, build

    def test_run_validation_fails_before_building_when_cmake_files_are_missing(self):
        result, report, build = self._run_validation(cmake_ok=False)
        self.assertFalse(result)
        build.assert_not_called()

    def test_run_validation_builds_when_nothing_was_lost(self):
        result, report, build = self._run_validation(removals=[[]])
        self.assertTrue(result)
        build.assert_called_once()

    def test_run_validation_lost_code_can_be_rechecked_then_accepted(self):
        finding = [("REDUCED", 2, 1, "f")]
        result, report, build = self._run_validation(removals=[finding, finding, []], answers=["", "r"])
        self.assertTrue(result)
        self.assertEqual(report.call_count, 3)

    def test_run_validation_lost_code_can_be_accepted_or_stop(self):
        finding = [("REDUCED", 2, 1, "f")]
        result, _, build = self._run_validation(removals=[finding], answers=["c"])
        self.assertTrue(result)
        result, _, build = self._run_validation(removals=[finding], answers=["s"])
        self.assertFalse(result)
        build.assert_not_called()

    def test_run_validation_reports_build_failure(self):
        result, _, _ = self._run_validation(removals=[[]], build_ok=False)
        self.assertFalse(result)


class BuildAndTestTests(TestCase):
    """build_and_test with fake build-webkit / run-webkit-tests scripts."""

    def setUp(self):
        super().setUp()
        self.scripts = self.tmp / "scripts"
        self.scripts.mkdir()
        self.record = self.tmp / "calls.txt"
        self.fail = self.tmp / "fail"   # a file whose content names the step that should fail
        self.fail.write_text("")
        for name in ("build-webkit", "run-webkit-tests"):
            path = self.scripts / name
            path.write_text(f'#!/bin/sh\necho "{name} $@" >> "{self.record}"\n'
                            f'case "$*" in *"$(cat "{self.fail}")"*) [ -n "$(cat "{self.fail}")" ] && exit 1;; esac\n'
                            f'echo "[1/1] step"\nexit 0\n')
            path.chmod(0o755)
        (self.tmp / "Makefile").write_text("release:\n\t@echo $(ASAN) $(LIBFUZZER) \"$(SCHEME)\" >> " + str(self.record) + "\n")
        patcher = mock.patch.object(u, "SCRIPTS_DIR", self.scripts)
        patcher.start()
        self.addCleanup(patcher.stop)

    def options(self, **overrides):
        values = dict(release=False, skip_build=False, build_only=False, tests=["webrtc"],
                      fuzzers=False, ios_simulator=False, ios_device=False)
        values.update(overrides)
        return types.SimpleNamespace(**values)

    def calls(self):
        return self.record.read_text().splitlines() if self.record.exists() else []

    def run_it(self, options):
        return u.build_and_test(self.tmp, options, self.tmp / "build.log")

    def test_default_is_a_cmake_debug_build_then_tests(self):
        self.no_questions()
        self.answers("n", "n", "n")
        self.assertTrue(self.run_it(self.options(fuzzers=None, ios_simulator=None, ios_device=None)))
        calls = self.calls()
        self.assertEqual(len(calls), 2)
        self.assertIn("build-webkit --cmake --debug --cmakeargs=", calls[0])
        self.assertIn("-Xlinker --no-demangle", calls[0])
        self.assertIn("run-webkit-tests --cmake --debug --no-build --no-show-results -f --child-processes=4 webrtc", calls[1])

    def test_release_flag(self):
        self.no_questions()
        self.assertTrue(self.run_it(self.options(release=True)))
        self.assertIn("--cmake --release", self.calls()[0])

    def test_optional_builds_are_asked_about_after_the_cmake_build(self):
        self.answers("y", "n", "y")   # fuzzers yes, simulator no, device yes
        options = self.options(fuzzers=None, ios_simulator=None, ios_device=None)
        self.assertTrue(self.run_it(options))
        calls = self.calls()
        self.assertEqual(len(calls), 4)   # CMake build, fuzzers, iOS device (no simulator), tests
        self.assertIn("--cmake --debug", calls[0])
        self.assertEqual(calls[1], "YES YES All Fuzzers")
        self.assertEqual(calls[2], "build-webkit --release --ios-device")
        self.assertIn("run-webkit-tests", calls[3])
        self.assertEqual((options.fuzzers, options.ios_simulator, options.ios_device), (True, False, True))

    def test_answers_are_remembered_for_retries(self):
        options = self.options(fuzzers=None, ios_simulator=None, ios_device=None)
        self.answers("n", "y", "n")
        self.assertTrue(self.run_it(options))
        self.no_questions()
        self.record.unlink()
        self.assertTrue(self.run_it(options))
        self.assertIn("build-webkit --release --ios-simulator", self.calls())

    def test_flags_answer_yes_without_asking(self):
        self.no_questions()
        self.assertTrue(self.run_it(self.options(ios_simulator=True, ios_device=True)))
        self.assertEqual([c.split()[0] + " " + " ".join(c.split()[1:3]) for c in self.calls()[:3]],
                         ["build-webkit --cmake --debug", "build-webkit --release --ios-simulator",
                          "build-webkit --release --ios-device"])

    def test_a_failing_step_stops_everything_after_it(self):
        self.fail.write_text("--ios-simulator")
        self.no_questions()
        self.assertFalse(self.run_it(self.options(ios_simulator=True, ios_device=True)))
        self.assertFalse(any("ios-device" in c or "run-webkit-tests" in c for c in self.calls()))
        self.assertIn("BUILD OF IOS SIMULATOR (RELEASE) FAILED", self.output.getvalue())

    def test_failing_tests(self):
        self.fail.write_text("--child-processes")
        self.no_questions()
        self.assertFalse(self.run_it(self.options()))
        self.assertIn("TESTS FAILED", self.output.getvalue())

    def test_skip_build_and_build_only(self):
        self.no_questions()
        self.assertTrue(self.run_it(self.options(skip_build=True)))
        self.assertEqual(len(self.calls()), 1)
        self.assertIn("run-webkit-tests", self.calls()[0])
        self.record.unlink()
        self.assertTrue(self.run_it(self.options(build_only=True)))
        self.assertEqual(len(self.calls()), 1)
        self.assertIn("build-webkit --cmake", self.calls()[0])

    def test_step_headings_are_not_indented(self):
        self.no_questions()
        self.run_it(self.options())
        shown = self.output.getvalue()
        for heading in ("Building WebKit with CMake (debug)...", "Running layout tests..."):
            self.assertIn("\n" + heading, "\n" + shown)


class ResyncEndToEndTests(GitTestCase):
    """The whole resync on a small repository, as the user would run it."""

    def setUp(self):
        super().setUp()
        text = "".join(f"l{i}\n" for i in range(1, 13))
        self.commit("Resync libwebrtc up to M100", {f"{ABSEIL}/a.cc": text, f"{WEBRTC}/w.cc": text,
                                                    f"{LIBYUV}/y.cc": "y\n", "Source/WebCore/x.cpp": "c\n"})
        self.base = self.git("rev-parse", "HEAD")
        self.bare = self.make_bare_remote()
        # The previous all-webrtc branch, with a WebKit patch.
        self.git("checkout", "-q", "-b", "tmp")
        self.commit("WebKit patch for abseil", {f"{ABSEIL}/a.cc": text.replace("l12\n", "l12 PATCH\n")})
        self.git("push", "-q", "fork", "tmp:all-webrtc-m152")
        self.git("checkout", "-q", "main")
        self.git("branch", "-q", "-D", "tmp")
        # Trunk commits made since the last resync.
        self.commit("Fix abseil in trunk", {f"{ABSEIL}/a.cc": text.replace("l6\n", "l6 TRUNK\n")},
                    author="Alice <alice@example.com>")
        self.commit("Mixed WebCore and webrtc change",
                    {f"{WEBRTC}/w.cc": text.replace("l9\n", "l9 TRUNK\n"), "Source/WebCore/x.cpp": "c\nmore\n"},
                    author="Bob <bob@example.com>")
        self.commit("Update libyuv to M143", {f"{LIBYUV}/y.cc": "y2\n"})
        # The new Chromium.
        self.chromium = self.tmp / "chromium" / "src"
        for name, target in (("abseil-cpp", ABSEIL), ("webrtc", WEBRTC)):
            (self.chromium / "third_party" / name).mkdir(parents=True)
        (self.chromium / "third_party/abseil-cpp/a.cc").write_text(text.replace("l1\n", "l1 UP\n"))
        (self.chromium / "third_party/webrtc/w.cc").write_text(text.replace("l1\n", "l1 UP\n"))

    def resync(self, *extra_answers, previous="m152", version="m155"):
        # base, which commits to replay, create branch, copy, apply the patch branch, its base, its commits
        self.answers("", "", "", "", "", "", "", *extra_answers)
        return u.resync_all_webrtc(self.root, self.chromium, version, previous, "fork")

    def test_full_resync(self):
        branch = self.resync()
        self.assertEqual(branch, "all-webrtc-m155")
        self.assertEqual(u.current_branch(self.root), "all-webrtc-m155")
        self.assertEqual(self.subjects(count=6)[:5], [
            "Mixed WebCore and webrtc change", "Fix abseil in trunk", "WebKit patch for abseil",
            "Resync webrtc from " + str(self.chromium / "third_party/webrtc"),
            "Resync abseil-cpp from " + str(self.chromium / "third_party/abseil-cpp")])
        text = self.read(f"{ABSEIL}/a.cc")
        self.assertIn("l1 UP", text)          # the new upstream
        self.assertIn("l12 PATCH", text)      # the patch from the previous branch
        self.assertIn("l6 TRUNK", text)       # the trunk change made since the last resync
        self.assertIn("l9 TRUNK", self.read(f"{WEBRTC}/w.cc"))
        self.assertEqual(self.read(f"{LIBYUV}/y.cc"), "y2\n")           # untouched: no libyuv in Chromium
        self.assertEqual(self.read("Source/WebCore/x.cpp"), "c\nmore\n")  # from trunk, not replayed again

    def test_replayed_commits_keep_author_title_and_only_the_webrtc_part(self):
        self.resync()
        self.assertEqual(self.git("log", "-1", "--format=%an <%ae>", "--grep=Fix abseil"), "Alice <alice@example.com>")
        self.assertEqual(self.git("log", "-1", "--format=%an", "--grep=Mixed"), "Bob")
        mixed = self.git("log", "-1", "--format=%H", "--grep=Mixed")
        self.assertEqual(self.git("show", "--name-only", "--format=", mixed).splitlines(), [f"{WEBRTC}/w.cc"])
        self.assertIn("(cherry picked from commit", self.git("log", "-1", "--format=%B", mixed))

    def test_libyuv_roll_is_not_replayed(self):
        self.resync()
        self.assertNotIn("Update libyuv to M143", self.subjects("main..HEAD"))

    def test_choosing_some_of_the_commits_to_replay(self):
        self.answers("", "2", "", "", "", "", "")   # only the second: "Mixed WebCore and webrtc change"
        self.assertEqual(u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork"), "all-webrtc-m155")
        on_branch = self.subjects("main..HEAD")
        self.assertIn("Mixed WebCore and webrtc change", on_branch)
        self.assertNotIn("Fix abseil in trunk", on_branch)
        self.assertIn("l9 TRUNK", self.read(f"{WEBRTC}/w.cc"))
        self.assertNotIn("l6 TRUNK", self.read(f"{ABSEIL}/a.cc"))

    def _trunk_commit_conflicting_with_the_new_upstream(self):
        text = "".join(f"l{i}\n" for i in range(1, 13))
        self.commit("Conflicting trunk change", {f"{ABSEIL}/a.cc": self.read(f"{ABSEIL}/a.cc").replace("l1\n", "l1 TRUNK\n")},
                    author="Carol <carol@example.com>")

    def test_a_replayed_commit_that_conflicts_can_be_skipped(self):
        self._trunk_commit_conflicting_with_the_new_upstream()
        self.answers("", "", "", "", "", "", "", "skip")
        self.assertEqual(u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork"), "all-webrtc-m155")
        subjects = self.subjects("main..HEAD")   # only what the resync added to the branch
        self.assertNotIn("Conflicting trunk change", subjects)
        self.assertIn("Fix abseil in trunk", subjects)
        self.assertIn("Mixed WebCore and webrtc change", subjects)
        self.assertIn("l1 UP", self.read(f"{ABSEIL}/a.cc"))
        self.assertEqual(u.conflicted_paths(self.root), [])

    def test_a_replayed_commit_that_conflicts_can_be_resolved(self):
        self._trunk_commit_conflicting_with_the_new_upstream()
        self.answers("", "", "", "", "", "", "")

        def resolve(message, default=None):
            path = f"{ABSEIL}/a.cc"
            self.write(path, self.read(path).split("<<<<<<<")[0] + "l1 MERGED\n"
                       + "".join(f"l{i}\n" for i in range(2, 13)).replace("l6\n", "l6 TRUNK\n").replace("l12\n", "l12 PATCH\n"))
            self.git("add", path)
            return "continue"

        real_prompt = u.prompt
        with mock.patch.object(u, "prompt", side_effect=lambda m, default=None:
                               resolve(m, default) if "resolved" in m else real_prompt(m, default)):
            self.assertEqual(u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork"), "all-webrtc-m155")
        self.assertEqual(self.subjects(count=1), ["Conflicting trunk change"])
        self.assertEqual(self.git("log", "-1", "--format=%an"), "Carol")
        self.assertIn("l1 MERGED", self.read(f"{ABSEIL}/a.cc"))

    def test_aborting_the_replay_leaves_the_resync_incomplete(self):
        self._trunk_commit_conflicting_with_the_new_upstream()
        self.answers("", "", "", "", "", "", "", "abort")
        self.assertIsNone(u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork"))
        self.assertIn("Replay did not complete", self.output.getvalue())
        self.assertIn("will not be offered for pushing", self.output.getvalue())

    def test_choosing_no_commits_to_replay(self):
        self.answers("", "n", "", "", "", "", "")   # base, replay: none, then the rest
        branch = u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork")
        self.assertEqual(branch, "all-webrtc-m155")
        self.assertNotIn("Fix abseil in trunk", self.subjects("main..HEAD"))
        self.assertNotIn("l6 TRUNK", self.read(f"{ABSEIL}/a.cc"))

    def test_blank_base_replays_nothing(self):
        self.answers("-", "", "", "", "", "", "")
        with mock.patch.object(u, "prompt", side_effect=lambda m, default=None: None if "Base commit:" in m else (default)):
            pass
        # A blank answer to the base question: prompt() returns the default for ""; typing a
        # ref that does not resolve must skip the replay instead.
        self.answers("not-a-ref", "", "", "", "", "")
        branch = u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork")
        self.assertEqual(branch, "all-webrtc-m155")
        self.assertIn("Could not resolve 'not-a-ref'", self.output.getvalue())
        self.assertNotIn("l6 TRUNK", self.read(f"{ABSEIL}/a.cc"))

    def test_second_resync_of_the_same_version_on_top_of_the_landed_first_one(self):
        self.resync()
        self.git("push", "-q", "fork", "all-webrtc-m155")
        # The first resync lands in trunk as a single commit; Chromium then moves to an updated m155.
        self.git("checkout", "-q", "main")
        self.git("merge", "-q", "--squash", "all-webrtc-m155")
        landed = self.commit("Resync libwebrtc up to M155 (first version)")
        self.git("branch", "-q", "-D", "all-webrtc-m155")
        text = "".join(f"l{i}\n" for i in range(1, 13))
        (self.chromium / "third_party/abseil-cpp/a.cc").write_text(text.replace("l3\n", "l3 UP2\n"))
        (self.chromium / "third_party/webrtc/w.cc").write_text(text.replace("l3\n", "l3 UP2\n"))

        # No commits since the landing to replay, so that question is not asked; the name is taken
        # on the remote, so the branch question is.
        branch = self.resync(previous="m155")
        self.assertEqual(branch, "all-webrtc-m155-2")
        self.assertIn("Branch 'all-webrtc-m155' already exists on remote 'fork'", self.output.getvalue())
        self.assertIn(f"Last commit that looks like a webrtc resync: {landed[:12]}", self.output.getvalue())
        abseil = self.read(f"{ABSEIL}/a.cc")
        self.assertIn("l3 UP2", abseil)       # the updated upstream
        self.assertNotIn("l1 UP", abseil)     # the first upstream is gone
        self.assertIn("l12 PATCH", abseil)    # the first branch's patch is back
        self.assertIn("l6 TRUNK", abseil)     # and its replayed trunk change
        self.assertIn("l9 TRUNK", self.read(f"{WEBRTC}/w.cc"))

    def test_declining_the_copy_leaves_the_starting_branch(self):
        self.answers("", "", "", "n")   # base, replay, create branch, copy: no
        self.assertIsNone(u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork"))
        self.assertEqual(u.current_branch(self.root), "main")

    def test_a_failed_patch_step_leaves_the_branch_incomplete(self):
        self.answers("", "", "", "", "", "", "")
        self.git("push", "-q", "-d", "fork", "all-webrtc-m152")      # the previous branch is gone
        self.assertIsNone(u.resync_all_webrtc(self.root, self.chromium, "m155", "m152", "fork"))
        self.assertIn("Patch step did not complete", self.output.getvalue())

    def test_libraries_missing_from_chromium_are_skipped(self):
        self.resync()
        self.assertIn("Skipping boringssl", self.output.getvalue())


class CommandLineTests(TestCase):
    def main(self, *arguments):
        with mock.patch.object(sys, "argv", ["update-libwebrtc.py", *arguments]):
            return u.main()

    def test_no_arguments_prints_the_overview(self):
        self.assertEqual(self.main(), 0)
        shown = self.output.getvalue()
        for text in ("update-chromium", "resync", "validate", "LONG TIME", "WebKitBuild/update-libwebrtc"):
            self.assertIn(text, shown)
        self.assertNotIn("{name}", shown)

    def test_help_matches_dash_dash_help(self):
        self.assertEqual(self.main("help"), 0)
        help_output = self.output.getvalue()
        self.output.truncate(0)
        self.output.seek(0)
        with self.assertRaises(SystemExit) as raised:
            self.main("--help")
        self.assertEqual(raised.exception.code, 0)
        self.assertEqual(self.output.getvalue(), help_output)

    def test_help_for_a_command(self):
        self.assertEqual(self.main("help", "validate"), 0)
        self.assertIn("--fuzzers", self.output.getvalue())
        self.assertIn("--ios-device", self.output.getvalue())

    def test_unknown_command_and_topic(self):
        with self.assertRaises(SystemExit) as raised, contextlib.redirect_stderr(io.StringIO()):
            self.main("bogus")
        self.assertEqual(raised.exception.code, 2)
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            self.main("help", "bogus")

    def test_validation_options_exist_on_resync_and_validate(self):
        for command in ("resync", "validate"):
            self.main("help", command)
            for option in ("--release", "--skip-build", "--build-only", "--tests", "--fuzzers", "--ios-simulator",
                           "--ios-device", "--no-push", "--webkit-remote", "--chrome-version"):
                self.assertIn(option, self.output.getvalue(), f"{command} {option}")
        self.assertIn("--previous-version", self.output.getvalue())

    def test_update_chromium_rejects_bad_arguments_without_touching_anything(self):
        root = self.tmp
        bad_version = types.SimpleNamespace(chrome_version="155", branch="7000", chromium_path=None, remote="origin")
        self.assertEqual(u.cmd_update_chromium(bad_version, root), 1)
        bad_branch = types.SimpleNamespace(chrome_version="m155", branch="branch_7000", chromium_path=None, remote="origin")
        self.assertEqual(u.cmd_update_chromium(bad_branch, root), 1)
        missing = types.SimpleNamespace(chrome_version="m155-2", branch="7000", chromium_path=str(self.tmp / "nope"),
                                        remote="origin")
        self.assertEqual(u.cmd_update_chromium(missing, root), 1)
        self.assertIn("No such directory", self.output.getvalue())
        not_git = self.tmp / "plain"
        not_git.mkdir()
        plain = types.SimpleNamespace(chrome_version="m155", branch="7000", chromium_path=str(not_git), remote="origin")
        self.assertEqual(u.cmd_update_chromium(plain, root), 1)
        self.assertIn("does not look like a git checkout", self.output.getvalue())

    def test_log_file_goes_under_webkitbuild(self):
        root = self.tmp / "logs-root"
        root.mkdir()
        saved = (u._log_file, u._log_path)
        try:
            path = u.open_log(root, "validate")
            u.log("hello")
            u._log_file.close()
        finally:
            u._log_file, u._log_path = saved
        self.assertEqual(path.parent, root / "WebKitBuild" / "update-libwebrtc")
        self.assertTrue(path.name.startswith("update-libwebrtc-validate-"))
        self.assertEqual(path.read_text(), "hello\n")

    def test_resolve_chrome_version(self):
        root = self.tmp / "r"
        root.mkdir()
        subprocess.run(["git", "init", "-q", "-b", "all-webrtc-m155-2", str(root)], check=True)
        subprocess.run(["git", "-C", str(root), "-c", "user.name=T", "-c", "user.email=t@t", "-c", "commit.gpgsign=false",
                        "commit", "-q", "--allow-empty", "-m", "init"], check=True)
        args = types.SimpleNamespace(chrome_version=None)
        self.no_questions()
        self.assertEqual(u.resolve_chrome_version(root, args, {"chrome_version": "m999"}), "m155-2")  # from the branch
        self.assertEqual(u.resolve_chrome_version(root, types.SimpleNamespace(chrome_version="m1"), {}), "m1")
        subprocess.run(["git", "-C", str(root), "checkout", "-q", "-b", "feature"], check=True)
        self.answers("")
        self.assertEqual(u.resolve_chrome_version(root, args, {"chrome_version": "m154"}), "m154")   # remembered


if __name__ == "__main__":
    unittest.main()
