# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import contextlib
import importlib.machinery
import importlib.util
import io
import json
import os
import subprocess
import tempfile
import unittest

# Tools/Scripts/check-test-discovery has no .py suffix, so load it by path.
SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "check-test-discovery")
loader = importlib.machinery.SourceFileLoader("check_test_discovery", SCRIPT)
tool = importlib.util.module_from_spec(importlib.util.spec_from_loader(loader.name, loader))
loader.exec_module(tool)


def line(text, number=1):
    return {"status": "effective", "expected": ["FAIL"], "path": text, "filename": "LayoutTests/TestExpectations",
            "line_number": number, "original_string": "Bug(x) " + text + " [ Failure ]", "from_skipped_list": False}


def entry(expected=("PASS",), lines=()):
    return {"expected": list(expected), "lines": list(lines)}


def snapshot(drivers):
    return {"version": tool.SNAPSHOT_VERSION, "drivers": drivers}


def drivers(run=None, skip=None, driver="WebKitTestRunner"):
    return {driver: {"skip": skip or {}, "run": run or {}}}


def rows(base, current):
    return list(tool.differences(tool.flatten(snapshot(base)), tool.flatten(snapshot(current))))


class FlattenTest(unittest.TestCase):
    def test_line_numbers_are_left_out(self):
        base = drivers(run={"default": {"a.html": entry(lines=[line("a.html", 3)])}})
        current = drivers(run={"default": {"a.html": entry(lines=[line("a.html", 40)])}})
        self.assertEqual(rows(base, current), [])

    def test_a_test_listed_twice_for_one_driver_is_an_error(self):
        listed_twice = drivers(run={"default": {"a.html": entry()}}, skip={"a.html": entry()})
        with self.assertRaises(tool.ToolError):
            tool.flatten(snapshot(listed_twice))


class DifferencesTest(unittest.TestCase):
    def test_no_difference(self):
        self.assertEqual(rows(drivers(run={"default": {"a.html": entry()}}), drivers(run={"default": {"a.html": entry()}})), [])

    def test_lost_test(self):
        found = rows(drivers(run={"default": {"a.html": entry()}}), drivers())
        self.assertEqual(found, [{"driver": "WebKitTestRunner", "test": "a.html",
                                  "previous": {"where": "default", "expected": ["PASS"], "lines": []}, "new": None}])

    def test_new_test(self):
        found = rows(drivers(), drivers(run={"default": {"a.html": entry()}}))
        self.assertEqual(found, [{"driver": "WebKitTestRunner", "test": "a.html",
                                  "previous": None, "new": {"where": "default", "expected": ["PASS"], "lines": []}}])

    def test_changed_expected_results(self):
        found = rows(drivers(run={"default": {"a.html": entry(["PASS"])}}), drivers(run={"default": {"a.html": entry(["SKIP"])}}))
        self.assertEqual([(row["previous"]["expected"], row["new"]["expected"]) for row in found], [(["PASS"], ["SKIP"])])

    def test_changed_effective_line(self):
        base = drivers(run={"default": {"a.html": entry(lines=[line("a.html")])}})
        current = drivers(run={"default": {"a.html": entry(lines=[dict(line("a.html"), original_string="Bug(y) a.html [ Failure ]")])}})
        self.assertEqual(len(rows(base, current)), 1)

    def test_a_move_between_device_types_is_one_row(self):
        base = drivers(run={"iPhone": {"a.html": entry()}, "iPad": {}})
        current = drivers(run={"iPhone": {}, "iPad": {"a.html": entry()}})
        found = rows(base, current)
        self.assertEqual([(row["previous"]["where"], row["new"]["where"]) for row in found], [("iPhone", "iPad")])

    def test_a_skipped_test_is_its_own_where(self):
        found = rows(drivers(skip={"a.html": entry()}), drivers(run={"default": {"a.html": entry()}}))
        self.assertEqual([(row["previous"]["where"], row["new"]["where"]) for row in found], [("skip", "default")])

    def test_drivers_are_kept_apart(self):
        base = {"A": {"skip": {}, "run": {"default": {"a.html": entry()}}}, "B": {"skip": {}, "run": {"default": {"a.html": entry()}}}}
        current = {"A": {"skip": {}, "run": {"default": {"a.html": entry()}}}, "B": {"skip": {}, "run": {"default": {"a.html": entry(["SKIP"])}}}}
        found = rows(base, current)
        self.assertEqual([row["driver"] for row in found], ["B"])

    def test_rows_are_sorted(self):
        found = rows(drivers(run={"default": {"b.html": entry(), "a.html": entry()}}), drivers())
        self.assertEqual([row["test"] for row in found], ["a.html", "b.html"])


class CommandTest(unittest.TestCase):
    """Runs the tool's main() against a stub run-webkit-tests in a temporary git checkout."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.checkout = os.path.join(self.directory.name, "checkout")
        os.makedirs(os.path.join(self.checkout, "Tools", "Scripts"))
        self.stub = os.path.join(self.checkout, "Tools", "Scripts", "run-webkit-tests")
        subprocess.run(["git", "init", "-q", self.checkout], check=True)
        self.set_output(drivers(run={"default": {"a.html": entry()}}))
        self.previous_directory = os.getcwd()
        os.chdir(self.checkout)
        self.addCleanup(os.chdir, self.previous_directory)

    def set_output(self, output):
        """Makes the stub print `output` as its JSON."""
        with open(self.stub, "w") as handle:
            handle.write("#!/bin/sh\ncat <<'EOF'\n" + json.dumps(output) + "\nEOF\n")
        os.chmod(self.stub, 0o755)

    def path(self, name):
        return os.path.join(self.directory.name, name)

    def run_main(self, *argv):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = tool.main(list(argv))
        return status, stdout.getvalue(), stderr.getvalue()

    def test_check_saves_a_versioned_snapshot_of_the_stub_output(self):
        self.assertEqual(self.run_main("check", "--save", self.path("base.json"))[0], 0)
        with open(self.path("base.json")) as handle:
            saved = json.load(handle)
        self.assertEqual(saved["version"], tool.SNAPSHOT_VERSION)
        self.assertEqual(saved["drivers"], drivers(run={"default": {"a.html": entry()}}))

    def test_check_runs_discovery_with_the_expected_options(self):
        self.run_main("check", "--save", self.path("base.json"), "--root", "/build", "--rwt-arg=--platform=mac")
        with open(self.path("base.json")) as handle:
            command = json.load(handle)["command"]
        for argument in ("--print-expectations", "--json", "--skipped=ignore", "--release", "--no-build", "--platform=mac"):
            self.assertIn(argument, command)
        self.assertEqual(command[command.index("--root") + 1], "/build")

    def test_check_against_an_unchanged_baseline_passes(self):
        self.run_main("check", "--save", self.path("base.json"))
        self.assertEqual(self.run_main("check", "--diff-to", self.path("base.json")), (0, "", ""))

    def test_check_lists_a_changed_expectation_and_fails(self):
        self.run_main("check", "--save", self.path("base.json"))
        self.set_output(drivers(run={"default": {"a.html": entry(["SKIP"])}}))
        status, out, _ = self.run_main("check", "--diff-to", self.path("base.json"))
        self.assertEqual(status, 1)
        self.assertEqual(json.loads(out)["new"]["expected"], ["SKIP"])

    def test_diff_compares_two_saved_snapshots(self):
        self.run_main("check", "--save", self.path("a.json"))
        self.set_output(drivers(run={"default": {"b.html": entry()}}))
        self.run_main("check", "--save", self.path("b.json"))
        status, out, _ = self.run_main("diff", self.path("a.json"), self.path("b.json"))
        self.assertEqual(status, 1)
        self.assertEqual(sorted((json.loads(line)["test"], json.loads(line)["previous"] is None) for line in out.splitlines()),
                         [("a.html", False), ("b.html", True)])

    def test_diff_of_identical_snapshots_passes(self):
        self.run_main("check", "--save", self.path("a.json"))
        self.assertEqual(self.run_main("diff", self.path("a.json"), self.path("a.json")), (0, "", ""))

    def test_check_needs_an_output(self):
        status, _, err = self.run_main("check")
        self.assertEqual(status, 2)
        self.assertIn("--save", err)

    def test_an_old_snapshot_version_is_rejected(self):
        with open(self.path("old.json"), "w") as handle:
            json.dump({"version": 2, "tests": {}}, handle)
        status, _, err = self.run_main("diff", self.path("old.json"), self.path("old.json"))
        self.assertEqual(status, 2)
        self.assertIn("version 3", err)

    def test_a_failing_stub_is_a_tool_error(self):
        with open(self.stub, "w") as handle:
            handle.write("#!/bin/sh\nexit 3\n")
        status, _, err = self.run_main("check", "--save", self.path("base.json"))
        self.assertEqual(status, 2)
        self.assertIn("status 3", err)


if __name__ == "__main__":
    unittest.main()
