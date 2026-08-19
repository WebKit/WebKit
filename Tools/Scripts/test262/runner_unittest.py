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
import re
import shutil
import sys
import tempfile
import unittest
from unittest import mock

import yaml

import runner


class TestParseMetadata(unittest.TestCase):
    def test_basic_metadata(self):
        contents = """\
/*---
description: A test
features: [SharedArrayBuffer]
flags: [noStrict]
negative:
  type: TypeError
  phase: parse
includes: [testTypedArray.js]
---*/
var x = 1;
"""
        meta = runner._parse_metadata(contents, "test.js")
        self.assertEqual(meta["description"], "A test")
        self.assertEqual(meta["features"], ["SharedArrayBuffer"])
        self.assertEqual(meta["flags"], ["noStrict"])
        self.assertEqual(meta["negative"]["type"], "TypeError")
        self.assertEqual(meta["includes"], ["testTypedArray.js"])

    def test_no_metadata(self):
        self.assertEqual(runner._parse_metadata("var x = 1;", "test.js"), {})

    def test_empty_metadata(self):
        contents = "/*---\n---*/"
        self.assertEqual(runner._parse_metadata(contents, "test.js"), {})

    def test_malformed_yaml(self):
        contents = "/*---\n: : : [invalid\n---*/"
        meta = runner._parse_metadata(contents, "test.js")
        self.assertEqual(meta, {})

    def test_multiple_features(self):
        contents = """\
/*---
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/
"""
        meta = runner._parse_metadata(contents, "test.js")
        self.assertEqual(
            meta["features"], ["Atomics", "SharedArrayBuffer", "TypedArray"]
        )


class TestParseError(unittest.TestCase):
    def test_none_input(self):
        self.assertIsNone(runner._parse_error(None))

    def test_empty_input(self):
        self.assertIsNone(runner._parse_error(""))

    def test_exception_format(self):
        output = "Exception: TypeError: undefined is not an object"
        self.assertEqual(
            runner._parse_error(output),
            "TypeError: undefined is not an object",
        )

    def test_exception_multiline(self):
        output = "some preamble\nException: RangeError: invalid array length\nmore stuff"
        self.assertEqual(
            runner._parse_error(output), "RangeError: invalid array length"
        )

    def test_no_exception_returns_first_line(self):
        output = "something went wrong\nsecond line"
        self.assertEqual(runner._parse_error(output), "something went wrong")


class TestDetectPlatform(unittest.TestCase):
    @mock.patch("runner.PlatformInfo")
    def test_macos_debug(self, mock_platform_info):
        mock_platform_info.return_value = mock.Mock(
            os_name="mac", os_version="15.4.1"
        )
        info = runner._detect_platform(release=False, sanitizer=None)
        self.assertEqual(info["os"], "macOS")
        self.assertEqual(info["os_version"], "15.4.1")
        self.assertEqual(info["build"], "debug")
        self.assertEqual(info["sanitizer"], "")

    @mock.patch("runner.PlatformInfo")
    def test_release_with_sanitizer(self, mock_platform_info):
        mock_platform_info.return_value = mock.Mock(
            os_name="mac", os_version="15.0"
        )
        info = runner._detect_platform(release=True, sanitizer="asan")
        self.assertEqual(info["build"], "release")
        self.assertEqual(info["sanitizer"], "asan")

    @mock.patch("runner.PlatformInfo")
    def test_linux(self, mock_platform_info):
        mock_platform_info.return_value = mock.Mock(
            os_name="linux", os_version="6.1.0"
        )
        info = runner._detect_platform(release=False, sanitizer=None)
        self.assertEqual(info["os"], "Linux")
        self.assertEqual(info["os_version"], "6.1.0")


class TestEvaluateCondition(unittest.TestCase):
    PLATFORM = {
        "os": "macOS",
        "os_version": "15.4.1",
        "build": "debug",
        "sanitizer": "asan",
    }

    def test_os_match(self):
        self.assertTrue(
            runner._evaluate_condition({"os": "macOS"}, self.PLATFORM)
        )

    def test_os_mismatch(self):
        self.assertFalse(
            runner._evaluate_condition({"os": "Linux"}, self.PLATFORM)
        )

    def test_os_case_insensitive(self):
        self.assertTrue(
            runner._evaluate_condition({"os": "macos"}, self.PLATFORM)
        )

    def test_os_version(self):
        self.assertTrue(
            runner._evaluate_condition(
                {"os_version": ">=15.0"}, self.PLATFORM
            )
        )
        self.assertFalse(
            runner._evaluate_condition(
                {"os_version": "<15.0"}, self.PLATFORM
            )
        )

    def test_os_version_missing(self):
        plat = dict(self.PLATFORM, os_version="")
        self.assertFalse(
            runner._evaluate_condition({"os_version": ">=15.0"}, plat)
        )

    def test_os_version_invalid(self):
        plat = dict(self.PLATFORM, os_version="not-a-version")
        self.assertFalse(
            runner._evaluate_condition({"os_version": ">=15.0"}, plat)
        )

    def test_build_match(self):
        self.assertTrue(
            runner._evaluate_condition({"build": "debug"}, self.PLATFORM)
        )
        self.assertFalse(
            runner._evaluate_condition({"build": "release"}, self.PLATFORM)
        )

    def test_sanitizer_match(self):
        self.assertTrue(
            runner._evaluate_condition({"sanitizer": "asan"}, self.PLATFORM)
        )
        self.assertFalse(
            runner._evaluate_condition({"sanitizer": "tsan"}, self.PLATFORM)
        )

    def test_multiple_conditions_all_must_match(self):
        self.assertTrue(
            runner._evaluate_condition(
                {"os": "macOS", "build": "debug"}, self.PLATFORM
            )
        )
        self.assertFalse(
            runner._evaluate_condition(
                {"os": "macOS", "build": "release"}, self.PLATFORM
            )
        )

    def test_empty_condition(self):
        self.assertTrue(runner._evaluate_condition({}, self.PLATFORM))


class TestShouldSkip(unittest.TestCase):
    PLATFORM = {
        "os": "macOS",
        "os_version": "15.4.1",
        "build": "debug",
        "sanitizer": "",
    }

    def _skip(self, rel_path, metadata=None, config=None, skip_set=None,
              skip_paths=None, skip_features=None, filter_features=None,
              conditional_skips=None):
        return runner._should_skip(
            rel_path,
            metadata or {},
            config or {},
            skip_set or set(),
            skip_paths or [],
            skip_features or set(),
            filter_features or set(),
            self.PLATFORM,
            conditional_skips or [],
        )

    def test_skip_by_file(self):
        self.assertTrue(
            self._skip(
                "test/foo.js",
                config={"skip": {"files": ["test/foo.js"]}},
                skip_set={"test/foo.js"},
            )
        )

    def test_skip_by_path_pattern(self):
        self.assertTrue(
            self._skip(
                "test/annexB/foo.js",
                config={"skip": {"paths": ["annexB"]}},
                skip_paths=["annexB"],
            )
        )

    def test_no_skip_without_config(self):
        self.assertFalse(self._skip("test/foo.js"))

    def test_skip_by_feature(self):
        self.assertTrue(
            self._skip(
                "test/foo.js",
                metadata={"features": ["Atomics"]},
                config={"skip": {"features": ["Atomics"]}},
                skip_features={"Atomics"},
            )
        )

    def test_filter_features_overrides_skip(self):
        self.assertFalse(
            self._skip(
                "test/foo.js",
                metadata={"features": ["Atomics"]},
                config={"skip": {"features": ["Atomics"]}},
                skip_features={"Atomics"},
                filter_features={"Atomics"},
            )
        )

    def test_filter_features_skips_non_matching(self):
        self.assertTrue(
            self._skip(
                "test/foo.js",
                metadata={"features": ["Proxy"]},
                config={"skip": {}},
                filter_features={"Atomics"},
            )
        )

    def test_filter_features_without_config(self):
        self.assertTrue(
            self._skip(
                "test/foo.js",
                metadata={"features": ["Proxy"]},
                filter_features={"Atomics"},
            )
        )
        self.assertFalse(
            self._skip(
                "test/foo.js",
                metadata={"features": ["Atomics"]},
                filter_features={"Atomics"},
            )
        )

    def test_conditional_skip_by_file(self):
        conds = [{"if": {"os": "macOS"}, "files": ["test/foo.js"]}]
        self.assertTrue(
            self._skip("test/foo.js", conditional_skips=conds)
        )

    def test_conditional_skip_not_matching_platform(self):
        conds = [{"if": {"os": "Linux"}, "files": ["test/foo.js"]}]
        self.assertFalse(
            self._skip("test/foo.js", conditional_skips=conds)
        )

    def test_conditional_skip_by_path(self):
        conds = [{"if": {"build": "debug"}, "paths": ["staging/"]}]
        self.assertTrue(
            self._skip("test/staging/foo.js", conditional_skips=conds)
        )

    def test_conditional_skip_by_feature(self):
        conds = [
            {"if": {"sanitizer": "asan"}, "features": ["SharedArrayBuffer"]}
        ]
        plat = dict(self.PLATFORM, sanitizer="asan")
        self.assertTrue(
            runner._should_skip(
                "test/foo.js",
                {"features": ["SharedArrayBuffer"]},
                {},
                set(),
                [],
                set(),
                set(),
                plat,
                conds,
            )
        )

    def test_conditional_skip_feature_overridden_by_filter(self):
        conds = [
            {"if": {"sanitizer": "asan"}, "features": ["SharedArrayBuffer"]}
        ]
        plat = dict(self.PLATFORM, sanitizer="asan")
        self.assertFalse(
            runner._should_skip(
                "test/foo.js",
                {"features": ["SharedArrayBuffer"]},
                {},
                set(),
                [],
                set(),
                {"SharedArrayBuffer"},
                plat,
                conds,
            )
        )


class TestGetScenarios(unittest.TestCase):
    def test_no_flags(self):
        self.assertEqual(
            runner._get_scenarios([]), ["strict mode", "default"]
        )

    def test_none_flags(self):
        self.assertEqual(
            runner._get_scenarios(None), ["strict mode", "default"]
        )

    def test_raw(self):
        self.assertEqual(runner._get_scenarios(["raw"]), ["raw"])

    def test_no_strict(self):
        self.assertEqual(runner._get_scenarios(["noStrict"]), ["default"])

    def test_only_strict(self):
        self.assertEqual(
            runner._get_scenarios(["onlyStrict"]), ["strict mode"]
        )

    def test_module(self):
        self.assertEqual(runner._get_scenarios(["module"]), ["module"])

    def test_unrecognized_flag(self):
        self.assertEqual(
            runner._get_scenarios(["async"]), ["strict mode", "default"]
        )

    def test_raw_takes_priority(self):
        self.assertEqual(
            runner._get_scenarios(["raw", "noStrict"]), ["raw"]
        )


class TestGetFeatureFlags(unittest.TestCase):
    def test_basic_feature_flag(self):
        config = {"flags": {"SharedArrayBuffer": "useSharedArrayBuffer"}}
        metadata = {"features": ["SharedArrayBuffer"]}
        flags = runner._get_feature_flags(config, metadata)
        self.assertEqual(flags, ["--useSharedArrayBuffer=1"])

    def test_multiple_features(self):
        config = {
            "flags": {
                "SharedArrayBuffer": "useSharedArrayBuffer",
                "Atomics": "useAtomics",
            }
        }
        metadata = {"features": ["SharedArrayBuffer", "Atomics"]}
        flags = runner._get_feature_flags(config, metadata)
        self.assertEqual(
            flags, ["--useSharedArrayBuffer=1", "--useAtomics=1"]
        )

    def test_unmapped_feature(self):
        config = {"flags": {"SharedArrayBuffer": "useSharedArrayBuffer"}}
        metadata = {"features": ["Proxy"]}
        self.assertEqual(runner._get_feature_flags(config, metadata), [])

    def test_no_features(self):
        config = {"flags": {"SharedArrayBuffer": "useSharedArrayBuffer"}}
        self.assertEqual(runner._get_feature_flags(config, {}), [])

    def test_no_config(self):
        self.assertEqual(runner._get_feature_flags({}, {"features": ["X"]}), [])

    def test_can_block_is_false(self):
        metadata = {"flags": ["CanBlockIsFalse"]}
        flags = runner._get_feature_flags({}, metadata)
        self.assertEqual(flags, ["--can-block-is-false"])

    def test_can_block_with_features(self):
        config = {"flags": {"SharedArrayBuffer": "useSharedArrayBuffer"}}
        metadata = {
            "features": ["SharedArrayBuffer"],
            "flags": ["CanBlockIsFalse"],
        }
        flags = runner._get_feature_flags(config, metadata)
        self.assertEqual(
            flags, ["--useSharedArrayBuffer=1", "--can-block-is-false"]
        )


class TestClassifyResults(unittest.TestCase):
    def _result(self, path="test/a.js", mode="default", passed=True,
                error=None, is_crash=False, skipped=False, exit_code=0):
        return {
            "path": path,
            "mode": mode,
            "passed": passed,
            "error": error,
            "is_crash": is_crash,
            "skipped": skipped,
            "exit_code": exit_code,
            "time": 0.1,
        }

    def test_expected_pass(self):
        r = self._result(passed=True)
        classified = runner._classify_results([r], {})
        self.assertEqual(classified[0]["result"], "expected_pass")

    def test_expected_pass_no_expectations(self):
        r = self._result(passed=True)
        classified = runner._classify_results([r], None)
        self.assertEqual(classified[0]["result"], "expected_pass")

    def test_unexpected_pass(self):
        r = self._result(passed=True)
        expectations = {"test/a.js": {"default": 3}}
        classified = runner._classify_results([r], expectations)
        self.assertEqual(classified[0]["result"], "unexpected_pass")

    def test_expected_fail_matching_exit_code(self):
        r = self._result(passed=False, exit_code=3)
        expectations = {"test/a.js": {"default": 3}}
        classified = runner._classify_results([r], expectations)
        self.assertEqual(classified[0]["result"], "expected_fail")

    def test_unexpected_fail_different_exit_code(self):
        r = self._result(passed=False, exit_code=139)
        expectations = {"test/a.js": {"default": 3}}
        classified = runner._classify_results([r], expectations)
        self.assertEqual(classified[0]["result"], "unexpected_fail")

    def test_unexpected_fail_no_expectations(self):
        r = self._result(passed=False, exit_code=3)
        classified = runner._classify_results([r], None)
        self.assertEqual(classified[0]["result"], "unexpected_fail")

    def test_unexpected_fail_no_entry(self):
        r = self._result(passed=False, exit_code=3)
        classified = runner._classify_results([r], {})
        self.assertEqual(classified[0]["result"], "unexpected_fail")

    def test_crash_can_match_expectation(self):
        # With exit-code-based expectations, a crash with a recorded exit
        # code (e.g. 139 for SIGSEGV) is no longer "always unexpected".
        r = self._result(passed=False, exit_code=139, is_crash=True)
        expectations = {"test/a.js": {"default": 139}}
        classified = runner._classify_results([r], expectations)
        self.assertEqual(classified[0]["result"], "expected_fail")

    def test_skipped(self):
        r = self._result(skipped=True)
        r["mode"] = "skip"
        classified = runner._classify_results([r], {})
        self.assertEqual(classified[0]["result"], "skip")

    def test_strict_mode_independent(self):
        r_default = self._result(mode="default", passed=True)
        r_strict = self._result(
            mode="strict mode", passed=False, exit_code=3
        )
        expectations = {"test/a.js": {"strict mode": 3}}
        classified = runner._classify_results(
            [r_default, r_strict], expectations
        )
        self.assertEqual(classified[0]["result"], "expected_pass")
        self.assertEqual(classified[1]["result"], "expected_fail")

    def test_pass_with_expected_zero_is_expected(self):
        # An expected_exit of 0 is unusual (we only record fails) but the
        # comparison should still work. The test passed, so since there is
        # an expectation entry it counts as an unexpected_pass.
        r = self._result(passed=True, exit_code=0)
        expectations = {"test/a.js": {"default": 0}}
        classified = runner._classify_results([r], expectations)
        self.assertEqual(classified[0]["result"], "unexpected_pass")


class TestCompileHarness(unittest.TestCase):
    def test_concatenates_files(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, "a.js"), "w") as f:
                f.write("var a = 1;\n")
            with open(os.path.join(d, "b.js"), "w") as f:
                f.write("var b = 2;\n")
            result = runner._compile_harness(["a.js", "b.js"], d)
            self.assertEqual(result, "var a = 1;\nvar b = 2;\n")


class TestCompileIncludes(unittest.TestCase):
    def test_creates_temp_file(self):
        with tempfile.TemporaryDirectory() as harness_dir:
            with open(os.path.join(harness_dir, "inc.js"), "w") as f:
                f.write("// include\n")
            with tempfile.TemporaryDirectory() as temp_dir:
                path = runner._compile_includes(
                    ["inc.js"], harness_dir, temp_dir
                )
                self.assertIsNotNone(path)
                with open(path) as f:
                    self.assertEqual(f.read(), "// include\n")

    def test_missing_include_returns_none(self):
        with tempfile.TemporaryDirectory() as harness_dir:
            with tempfile.TemporaryDirectory() as temp_dir:
                path = runner._compile_includes(
                    ["nonexistent.js"], harness_dir, temp_dir
                )
                self.assertIsNone(path)

    def test_multiple_includes(self):
        with tempfile.TemporaryDirectory() as harness_dir:
            with open(os.path.join(harness_dir, "a.js"), "w") as f:
                f.write("A")
            with open(os.path.join(harness_dir, "b.js"), "w") as f:
                f.write("B")
            with tempfile.TemporaryDirectory() as temp_dir:
                path = runner._compile_includes(
                    ["a.js", "b.js"], harness_dir, temp_dir
                )
                with open(path) as f:
                    self.assertEqual(f.read(), "AB")


class TestDiscoverTests(unittest.TestCase):
    def test_finds_js_files(self):
        with tempfile.TemporaryDirectory() as d:
            test_dir = os.path.join(d, "test")
            os.makedirs(os.path.join(test_dir, "sub"))
            for name in ["a.js", "b.JS", "c.txt", "d_FIXTURE.js"]:
                with open(os.path.join(test_dir, "sub", name), "w") as f:
                    f.write("")
            files = runner._discover_tests(d, ["test"])
            basenames = [os.path.basename(f) for f in files]
            self.assertIn("a.js", basenames)
            self.assertIn("b.JS", basenames)
            self.assertNotIn("c.txt", basenames)
            self.assertNotIn("d_FIXTURE.js", basenames)

    def test_missing_directory(self):
        with tempfile.TemporaryDirectory() as d:
            files = runner._discover_tests(d, ["nonexistent"])
            self.assertEqual(files, [])

    def test_sorted_output(self):
        with tempfile.TemporaryDirectory() as d:
            test_dir = os.path.join(d, "test")
            os.makedirs(test_dir)
            for name in ["c.js", "a.js", "b.js"]:
                with open(os.path.join(test_dir, name), "w") as f:
                    f.write("")
            files = runner._discover_tests(d, ["test"])
            basenames = [os.path.basename(f) for f in files]
            self.assertEqual(basenames, ["a.js", "b.js", "c.js"])


class TestLoadImportFile(unittest.TestCase):
    def test_parses_added_and_modified(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, "latest-changes-summary.txt"), "w") as f:
                f.write("A\ttest/foo.js\n")
                f.write("M\ttest/bar.js\n")
                f.write("D\ttest/deleted.js\n")
                f.write("A\tsrc/other.js\n")
            files = runner._load_import_file(d)
            self.assertEqual(len(files), 2)
            self.assertTrue(files[0].endswith("test/foo.js"))
            self.assertTrue(files[1].endswith("test/bar.js"))


class TestSaveExpectations(unittest.TestCase):
    def _make_result(self, path, mode, result, exit_code=0, skipped=False):
        return {
            "path": path,
            "mode": mode,
            "result": result,
            "exit_code": exit_code,
            "skipped": skipped,
        }

    def test_full_save(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml",
                                         delete=False) as f:
            fname = f.name
        try:
            results = [
                self._make_result("test/a.js", "default", "expected_fail", 3),
                self._make_result("test/a.js", "strict mode", "expected_pass"),
                self._make_result("test/b.js", "default", "unexpected_fail", 139),
            ]
            runner._save_expectations(fname, results, running_all=True)
            import yaml
            with open(fname) as f:
                saved = yaml.safe_load(f)
            self.assertIn("test/a.js", saved)
            self.assertEqual(saved["test/a.js"]["default"], 3)
            self.assertNotIn("strict mode", saved["test/a.js"])
            self.assertIn("test/b.js", saved)
            self.assertEqual(saved["test/b.js"]["default"], 139)
        finally:
            os.unlink(fname)

    def test_documents_exit_codes(self):
        import yaml
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml",
                                         delete=False) as f:
            fname = f.name
        try:
            results = [
                self._make_result("test/a.js", "default", "expected_fail", 3),
            ]
            # The header explains the exit codes to anyone editing the file, and
            # rewriting the file must not accumulate copies of it.
            runner._save_expectations(fname, results, running_all=True)
            runner._save_expectations(fname, results, running_all=True)
            with open(fname) as f:
                text = f.read()
            self.assertTrue(text.startswith("# Expected test262 failures"))
            self.assertEqual(text.count("# Expected test262 failures"), 1)
            self.assertEqual(yaml.safe_load(text), {"test/a.js": {"default": 3}})
        finally:
            os.unlink(fname)

    def test_incremental_preserves_unrun(self):
        import yaml
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml",
                                         delete=False) as f:
            yaml.dump({
                "test/old.js": {"default": 3},
                "test/a.js": {"default": 3, "strict mode": 139},
            }, f)
            fname = f.name
        try:
            results = [
                self._make_result("test/a.js", "default", "expected_pass"),
            ]
            runner._save_expectations(fname, results, running_all=False)
            with open(fname) as f:
                saved = yaml.safe_load(f)
            self.assertIn("test/old.js", saved)
            self.assertEqual(saved["test/old.js"]["default"], 3)
            self.assertIn("test/a.js", saved)
            self.assertEqual(saved["test/a.js"]["strict mode"], 139)
            self.assertNotIn("default", saved["test/a.js"])
        finally:
            os.unlink(fname)

    def test_incremental_adds_new_failure(self):
        import yaml
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml",
                                         delete=False) as f:
            yaml.dump({"test/old.js": {"default": 3}}, f)
            fname = f.name
        try:
            results = [
                self._make_result("test/new.js", "default",
                                  "unexpected_fail", 3),
            ]
            runner._save_expectations(fname, results, running_all=False)
            with open(fname) as f:
                saved = yaml.safe_load(f)
            self.assertIn("test/old.js", saved)
            self.assertIn("test/new.js", saved)
            self.assertEqual(saved["test/new.js"]["default"], 3)
        finally:
            os.unlink(fname)

    def test_incremental_removes_now_passing(self):
        import yaml
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml",
                                         delete=False) as f:
            yaml.dump({
                "test/a.js": {"default": 3},
            }, f)
            fname = f.name
        try:
            results = [
                self._make_result("test/a.js", "default", "expected_pass"),
            ]
            runner._save_expectations(fname, results, running_all=False)
            with open(fname) as f:
                saved = yaml.safe_load(f)
            self.assertFalse(saved)
        finally:
            os.unlink(fname)

    def test_incremental_skipped_not_counted_as_run(self):
        import yaml
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml",
                                         delete=False) as f:
            yaml.dump({
                "test/a.js": {"default": 3},
            }, f)
            fname = f.name
        try:
            results = [
                self._make_result("test/a.js", "skip", "skip", skipped=True),
            ]
            runner._save_expectations(fname, results, running_all=False)
            with open(fname) as f:
                saved = yaml.safe_load(f)
            self.assertIn("test/a.js", saved)
            self.assertEqual(saved["test/a.js"]["default"], 3)
        finally:
            os.unlink(fname)


class TestRunJsc(unittest.TestCase):
    def _base_env(self):
        return {"DYLD_FRAMEWORK_PATH": "/path/to/build", "TZ": "PST"}

    @mock.patch("runner.subprocess.run")
    def test_pass_returns_none_output(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="some debug output\n")
        code, output, _ = runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        self.assertEqual(code, 0)
        self.assertIsNone(output)

    @mock.patch("runner.subprocess.run")
    def test_failure_returns_output(self, mock_run):
        mock_run.return_value = mock.Mock(
            returncode=3,
            stdout="Exception: TypeError: undefined is not an object\n",
        )
        code, output, _ = runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        self.assertEqual(code, 3)
        self.assertIn("TypeError", output)

    @mock.patch("runner.subprocess.run")
    def test_uses_env_and_fork_exec(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        kwargs = mock_run.call_args[1]
        # shell=False keeps us off /bin/sh
        self.assertFalse(kwargs.get("shell", False))
        # env is passed directly
        self.assertEqual(kwargs["env"]["DYLD_FRAMEWORK_PATH"], "/path/to/build")
        self.assertEqual(kwargs["env"]["TZ"], "PST")
        # stderr redirected to stdout, matching 2>&1
        self.assertEqual(kwargs["stderr"], runner.subprocess.STDOUT)

    @mock.patch("runner.subprocess.run")
    def test_strict_mode_uses_strict_file(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "strict mode", {}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        cmd = mock_run.call_args[0][0]
        self.assertTrue(any(a.startswith("--strict-file=") for a in cmd))

    @mock.patch("runner.subprocess.run")
    def test_module_uses_module_file(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "module", {}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        cmd = mock_run.call_args[0][0]
        self.assertTrue(any(a.startswith("--module-file=") for a in cmd))

    @mock.patch("runner.subprocess.run")
    def test_raw_skips_harness(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "raw", {}, [],
            "/harness.js", "/async.js", None, None, self._base_env(),
        )
        cmd = mock_run.call_args[0][0]
        self.assertNotIn("/harness.js", cmd)
        self.assertNotIn("/async.js", cmd)

    @mock.patch("runner.subprocess.run")
    def test_async_includes_async_harness(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "default",
            {"flags": ["async"]}, [],
            "/harness.js", "/async.js", None, None, self._base_env(),
        )
        cmd = mock_run.call_args[0][0]
        self.assertIn("--test262-async", cmd)
        self.assertIn("/async.js", cmd)

    @mock.patch("runner.subprocess.run")
    def test_negative_exception(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "default",
            {"negative": {"type": "SyntaxError"}}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        cmd = mock_run.call_args[0][0]
        self.assertIn("--exception=SyntaxError", cmd)

    @mock.patch("runner.subprocess.run")
    def test_watchdog_timeout(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {}, [],
            "/harness.js", None, None, 10000, self._base_env(),
        )
        cmd = mock_run.call_args[0][0]
        self.assertIn("--watchdog=10000", cmd)

    @mock.patch("runner.subprocess.run")
    def test_feature_flags_in_command(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {},
            ["--useSharedArrayBuffer=1"],
            "/harness.js", None, None, None, self._base_env(),
        )
        cmd = mock_run.call_args[0][0]
        self.assertIn("--useSharedArrayBuffer=1", cmd)

    @mock.patch("runner.subprocess.run")
    def test_timeout_exception(self, mock_run):
        mock_run.side_effect = runner.subprocess.TimeoutExpired(cmd="jsc", timeout=300)
        code, output, _ = runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        self.assertEqual(code, 1)
        self.assertEqual(output, "Timeout")

    @mock.patch("runner.subprocess.run")
    def test_no_dyld_without_darwin(self, mock_run):
        mock_run.return_value = mock.Mock(returncode=0, stdout="")
        runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {}, [],
            "/harness.js", None, None, None, {"TZ": "PST"},
        )
        env = mock_run.call_args[1]["env"]
        self.assertNotIn("DYLD_FRAMEWORK_PATH", env)
        self.assertEqual(env["TZ"], "PST")

    @mock.patch("runner.subprocess.run")
    def test_signal_exit_code_normalized(self, mock_run):
        # Python reports signal kills as negative returncodes; the runner
        # should normalize -11 (SIGSEGV) to 139 (128+11) so that the shell-
        # style `exit_code & 0x7f` crash detection matches.
        mock_run.return_value = mock.Mock(returncode=-11, stdout="crashy")
        code, _, _ = runner._run_jsc(
            "/path/to/jsc", "/test.js", "default", {}, [],
            "/harness.js", None, None, None, self._base_env(),
        )
        self.assertEqual(code, 139)


class TestCrashDetection(unittest.TestCase):
    """Verify the exit_code & 0x7f signal extraction logic used in _process_test_file."""

    def test_exit_zero_no_crash(self):
        self.assertEqual(0 & 0x7f, 0)

    def test_exit_three_special_cased(self):
        signal = 3 & 0x7f
        self.assertEqual(signal, 3)
        if signal == 3:
            signal = 0
        self.assertEqual(signal, 0)

    def test_sigsegv_detected(self):
        # Shell reports 128 + 11 = 139 for SIGSEGV
        signal = 139 & 0x7f
        self.assertEqual(signal, 11)
        self.assertNotEqual(signal, 0)

    def test_sigabrt_detected(self):
        # Shell reports 128 + 6 = 134 for SIGABRT
        signal = 134 & 0x7f
        self.assertEqual(signal, 6)

    def test_sigbus_detected(self):
        # Shell reports 128 + 10 = 138 for SIGBUS
        signal = 138 & 0x7f
        self.assertEqual(signal, 10)


class TestSaveResults(unittest.TestCase):
    def test_round_trip(self):
        import yaml
        results = [
            {
                "path": "test/a.js",
                "mode": "default",
                "time": 0.5,
                "result": "expected_pass",
                "error": None,
                "output": None,
                "features": ["Proxy"],
            },
            {
                "path": "test/b.js",
                "mode": "strict mode",
                "time": 1.2,
                "result": "unexpected_fail",
                "error": "TypeError: x",
                "output": "Exception: TypeError: x",
                "features": [],
            },
        ]
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml",
                                         delete=False) as f:
            fname = f.name
        try:
            runner._save_results(fname, results)
            with open(fname) as f:
                loaded = yaml.safe_load(f)
            self.assertEqual(len(loaded), 2)
            self.assertEqual(loaded[0]["path"], "test/a.js")
            self.assertEqual(loaded[0]["result"], "expected_pass")
            self.assertNotIn("error", loaded[0])
            self.assertNotIn("output", loaded[0])
            self.assertEqual(loaded[1]["error"], "TypeError: x")
            self.assertEqual(loaded[1]["output"], "Exception: TypeError: x")
        finally:
            os.unlink(fname)


class TestFindJsc(unittest.TestCase):
    def test_explicit_path(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            fname = f.name
        try:
            result = runner._find_jsc(fname, release=False)
            self.assertEqual(result, os.path.abspath(fname))
        finally:
            os.unlink(fname)

    def test_explicit_path_missing(self):
        with self.assertRaises(SystemExit):
            runner._find_jsc("/nonexistent/jsc", release=False)

    def test_port_build_dir(self):
        with tempfile.TemporaryDirectory() as d:
            bin_dir = os.path.join(d, "WPE", "Release", "bin")
            os.makedirs(bin_dir)
            jsc_path = os.path.join(bin_dir, "jsc")
            open(jsc_path, "w").close()
            with mock.patch.dict(os.environ, {"WEBKIT_OUTPUTDIR": d}):
                result = runner._find_jsc(None, release=True, port="wpe")
            self.assertEqual(result, jsc_path)

    def test_port_build_dir_missing(self):
        with tempfile.TemporaryDirectory() as d:
            with mock.patch.dict(os.environ, {"WEBKIT_OUTPUTDIR": d}):
                with self.assertRaises(SystemExit):
                    runner._find_jsc(None, release=True, port="gtk")

    def test_port_does_not_fall_back_to_path(self):
        with tempfile.TemporaryDirectory() as d:
            with mock.patch.dict(os.environ, {"WEBKIT_OUTPUTDIR": d}):
                with mock.patch("runner.subprocess.run") as run_mock:
                    with self.assertRaises(SystemExit):
                        runner._find_jsc(None, release=True, port="jsc-only")
            run_mock.assert_not_called()


class TestTestFileRegex(unittest.TestCase):
    def test_matches_js(self):
        self.assertIsNotNone(runner._TEST_FILE_RE.search("test.js"))
        self.assertIsNotNone(runner._TEST_FILE_RE.search("test.JS"))

    def test_rejects_fixture(self):
        self.assertIsNone(runner._TEST_FILE_RE.search("test_FIXTURE.js"))

    def test_rejects_non_js(self):
        self.assertIsNone(runner._TEST_FILE_RE.search("test.py"))
        self.assertIsNone(runner._TEST_FILE_RE.search("test.txt"))


class TestParseArgs(unittest.TestCase):
    def _parse(self, *argv):
        with mock.patch.object(sys, "argv", ["test262-runner", *argv]):
            return runner._parse_args()

    def test_build_config_optional_defaults_to_release(self):
        # The original runner did not require --release/--debug and defaulted to
        # Release; --stats and --jsc must work without a build flag.
        self.assertTrue(self._parse().release)
        self.assertTrue(self._parse("--stats").release)
        self.assertTrue(self._parse("--jsc", "/path/to/jsc").release)

    def test_explicit_release(self):
        self.assertTrue(self._parse("--release").release)

    def test_explicit_debug(self):
        self.assertFalse(self._parse("--debug").release)

    def test_release_and_debug_are_mutually_exclusive(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                self._parse("--release", "--debug")


# Stands in for jsc. Picks its outcome from the name of the test file it is
# handed, so one mock covers every case below. The exit codes match jsc's:
# 3 is EXIT_EXCEPTION, 139 is death by SIGSEGV as reported through the shell.
_MOCK_JSC = """\
#!/usr/bin/env python3
import sys

target = sys.argv[-1]
if "crashing" in target:
    sys.stdout.write("Exception: Test262: This test crashes.")
    sys.exit(139)
if "failing" in target:
    sys.stdout.write("Exception: Test262: This test fails.")
    sys.exit(3)
sys.exit(0)
"""


class TestEndToEnd(unittest.TestCase):
    """Drives main() end to end against a mock jsc.

    Ported from Tools/Scripts/webkitperl/test262_unittest, which covered the
    Perl runner the same way and was removed along with it.
    """

    TEST_FILES = ("passing.js", "failing.js", "crashing.js")

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, True)

        self.test262_dir = os.path.join(self.tmp, "test262")
        harness_dir = os.path.join(self.test262_dir, "harness")
        tests_dir = os.path.join(self.test262_dir, "test")
        os.makedirs(harness_dir)
        os.makedirs(tests_dir)
        for name in ("sta.js", "assert.js", "doneprintHandle.js"):
            with open(os.path.join(harness_dir, name), "w") as f:
                f.write("// mock harness\n")
        for name in self.TEST_FILES:
            with open(os.path.join(tests_dir, name), "w") as f:
                f.write("// mock test\n")

        self.jsc = os.path.join(self.tmp, "mock-jsc")
        with open(self.jsc, "w") as f:
            f.write(_MOCK_JSC)
        os.chmod(self.jsc, 0o755)

        # main() writes test262-results/ into the working directory.
        self.addCleanup(os.chdir, os.getcwd())
        os.chdir(self.tmp)

    def _write_yaml(self, name, contents):
        path = os.path.join(self.tmp, name)
        with open(path, "w") as f:
            yaml.dump(contents, f)
        return path

    def _run(self, *extra_args, child_processes=("-p", "1")):
        argv = [
            "test262-runner",
            *child_processes,
            "--release",
            "--no-progress",
            "--jsc", self.jsc,
            "-t", self.test262_dir,
        ] + list(extra_args)
        out = io.StringIO()
        exit_code = 0
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            with mock.patch.object(sys, "argv", argv):
                try:
                    runner.main()
                except SystemExit as e:
                    exit_code = e.code or 0
        return exit_code, out.getvalue()

    def test_failure_without_expectations(self):
        code, out = self._run("-i", "-x", "-o", "test/failing.js")
        self.assertEqual(code, 1)
        self.assertEqual(out.count("! NEW FAIL"), 2)

    def test_pass_without_expectations(self):
        code, out = self._run("-i", "-x", "-o", "test/passing.js")
        self.assertEqual(code, 0)
        self.assertEqual(out.count("! NEW FAIL"), 0)

    def test_newly_failing(self):
        expectations = self._write_yaml(
            "expectations.yaml", {"test/unrelated.js": {"default": 3}}
        )
        code, out = self._run("-i", "-e", expectations, "-o", "test/failing.js")
        self.assertEqual(code, 1)
        self.assertEqual(out.count("! NEW FAIL"), 2)

    def test_newly_passing(self):
        expectations = self._write_yaml(
            "expectations.yaml",
            {"test/passing.js": {"default": 3, "strict mode": 3}},
        )
        code, out = self._run("-i", "-e", expectations, "-o", "test/passing.js")
        self.assertEqual(code, 0)
        self.assertEqual(out.count("NEW PASS"), 2)
        self.assertIn("2 tests newly pass", out)

    def test_expected_failure_is_not_new(self):
        expectations = self._write_yaml(
            "expectations.yaml",
            {"test/failing.js": {"default": 3, "strict mode": 3}},
        )
        code, out = self._run("-i", "-e", expectations, "-o", "test/failing.js")
        self.assertEqual(code, 0)
        self.assertEqual(out.count("! NEW FAIL"), 0)

    def test_crash_where_failure_was_expected_is_new(self):
        # A test that used to throw and now crashes is still a regression, even
        # though it failed before and fails now.
        expectations = self._write_yaml(
            "expectations.yaml",
            {"test/crashing.js": {"default": 3, "strict mode": 3}},
        )
        code, out = self._run("-i", "-e", expectations, "-o", "test/crashing.js")
        self.assertEqual(code, 1)
        self.assertEqual(out.count("! NEW FAIL"), 2)

    def test_test_only_accepts_a_single_file(self):
        code, out = self._run("-i", "-x", "-o", "test/passing.js")
        self.assertEqual(code, 0)
        self.assertIn("2 tests run", out)

    def test_no_tests_found_is_an_error(self):
        code, out = self._run("-i", "-x", "-o", "test/does-not-exist")
        self.assertEqual(code, 1)
        self.assertIn("No test files found.", out)

    def test_skipped_files_reports_newly_passing(self):
        config = self._write_yaml(
            "config.yaml", {"skip": {"files": ["test/passing.js"]}}
        )
        code, out = self._run("-c", config, "-S")
        self.assertIn("2 tests newly pass", out)
        self.assertIn("PASS test/passing.js", out)

    def test_skipped_files_does_not_report_expected_failures(self):
        config = self._write_yaml(
            "config.yaml", {"skip": {"files": ["test/failing.js"]}}
        )
        code, out = self._run("-c", config, "-S")
        self.assertEqual(out.count("! NEW FAIL"), 0)

    def test_save_records_exit_codes(self):
        expectations = os.path.join(self.tmp, "saved.yaml")
        self._run("-i", "-x", "--save", "-e", expectations)
        with open(expectations) as f:
            saved = yaml.safe_load(f)
        self.assertEqual(
            saved,
            {
                "test/crashing.js": {"default": 139, "strict mode": 139},
                "test/failing.js": {"default": 3, "strict mode": 3},
            },
        )

    def _child_processes(self, number_of_processors, *extra_args):
        with mock.patch.dict(os.environ, {"NUMBER_OF_PROCESSORS": number_of_processors}):
            code, out = self._run("-i", "-x", "-o", "test/passing.js", *extra_args, child_processes=())
        self.assertEqual(code, 0)
        match = re.search(r"^Child Processes: (\d+)$", out, re.MULTILINE)
        self.assertIsNotNone(match, out)
        return int(match.group(1))

    def test_number_of_processors_is_honored(self):
        # Linux CI bots cap the parallelism of each container with this env var. Ensure it is honored.
        self.assertEqual(self._child_processes("3"), 3)
        self.assertEqual(self._child_processes("5"), 5)

    def test_child_processes_option_overrides_number_of_processors(self):
        self.assertEqual(self._child_processes("3", "-p", "1"), 1)


if __name__ == "__main__":
    unittest.main()
