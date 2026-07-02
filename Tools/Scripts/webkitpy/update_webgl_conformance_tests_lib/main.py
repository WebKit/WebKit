#!/usr/bin/env python3
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Generates WebKit WebGL layout tests from the Khronos WebGL conformance tests

 To use this, get a copy of the WebGL conformance tests then run this script
 eg.
   cd ~/temp
   git clone git://github.com/KhronosGroup/WebGL.git
   mkdir backup
   mv ~/WebKit/LayoutTests/{http/,}webgl/{1.0.x,2.0.y,resources/webgl_test_files} backup
   update-webgl-conformance-tests -c ~/temp/WebGL
   run-webkit-tests --debug --webgl --order=random webgl
   run-webkit-tests --release --webgl --order=random webgl
   check-for-duplicated-platform-test-results -n 2>&1 | grep webgl
   [ Update TestExpectations ]
   [ Check in the result ]
"""

# Note: The script is currently not tested to be run on Windows.
#
# To add a new WebGL test suite, edit the main().
#
# python3 -m mypy main.py && python3 -m black main.py --line-length 200

import argparse
import os
import shutil
import sys

from distutils.version import StrictVersion as Version
from fnmatch import fnmatch
from pathlib import Path
from typing import Callable, Tuple, Iterable, Union, NamedTuple, Optional, Generator

_tool_dir = Path(__file__).parent
_layout_tests_dir = Path(__file__).parent.parent.parent.parent.parent / "LayoutTests"

_use_verbose_output = False

_clean_target_dirs = False

_IgnoredFilenameMatcher = Callable[[Path], bool]


# Creates an ignore file name pattern matcher based on list of match rules.
# Rule iteration order is the order of least specific to most specific rule.
# Empty list means do not ignore.
# If rule is str and it matches, it means ignore if no more specific rule matches.
# If rule is a tuple and its first element matches, it it means do not ignore if no more specific rule matches.
def _make_ignore_fnmatch_rule_matcher(rules: Iterable[Union[str, Tuple[str]]]) -> _IgnoredFilenameMatcher:
    def match_rules(name: Path) -> bool:
        should_filter = None
        for rule in rules:
            pattern: str = rule if isinstance(rule, str) else rule[0]
            if fnmatch(str(name), pattern):
                should_filter = isinstance(rule, str)
        return should_filter is True

    return match_rules


_file_patterns = _make_ignore_fnmatch_rule_matcher(
    [
        ".git",
        "*.pyc",
        "tmp*",
        "*/py/*",
        "*.py",
        "*/performance/*",
        "*/*.md",
        "*/extra/*",
        "*/00_test_list.txt",
        "*/CONFORMANCE_RULES.txt",
        "*/deqp/build.py",
        "*/deqp/compiler_additional_extern.js",
        "*/deqp/compiler.jar",
        "*/deqp/genHTMLfromTest.py",
        "*/deqp/run-closure.sh",
        "*/deqp/temp_externs*",
        "*/deqp/test-webgl2.js",
        "*/deqp/test-webgl2.sh",
        "*/textures/misc/origin-clean-conformance-offscreencanvas.html",
        "*/textures/misc/origin-clean-conformance.html",
        "*/more/functions/readPixelsBadArgs.html",
        "*/more/functions/texImage2DHTML.html",
        "*/more/functions/texSubImage2DHTML.html",
    ]
)

_http_file_patterns = _make_ignore_fnmatch_rule_matcher(
    [
        "*",
        ("*/textures/misc/origin-clean-conformance-offscreencanvas.html", ),
        ("*/textures/misc/origin-clean-conformance.html", ),
        ("*/more/functions/readPixelsBadArgs.html", ),
        ("*/more/functions/texImage2DHTML.html", ),
        ("*/more/functions/texSubImage2DHTML.html", ),
        ("*/more/unit.css", ),
        ("*/more/unit.js", ),
        ("*/more/util.js", ),
        ("*/js/js-test-post.js", ),
        ("*/js/js-test-pre.js", ),
        ("*/js/webgl-test-utils.js", ),
        ("*/resources/js-test-style.css", ),
        ("*/resources/opengl_logo.jpg", ),
        ("*/resources/thunderbird-logo-64x64.png", ),
    ]
)


def _copy_tree(src: Path, dst: Path, ignore: Optional[_IgnoredFilenameMatcher] = None):
    """Recursively copy a directory tree"""
    names = os.listdir(src)
    for name in names:
        src_name = src / name
        dst_name = dst / name

        if src_name.is_dir():
            _copy_tree(src_name, dst_name, ignore)
        else:
            if ignore is not None and ignore(src_name):
                if _use_verbose_output:
                    print("Ignoring: %s, matches ignore filter" % (src_name))
                continue
            if _use_verbose_output:
                print("Copying: %s -> %s" % (src_name, dst_name))
            dst.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src_name, dst_name)


class _WebGLTest(NamedTuple):
    path: Path
    min_version: Version
    max_version: Version


def _parse_webgl_tests(test_list_root: Path, test_list: Path, min_version: Version, max_version: Version) -> Generator[_WebGLTest, None, None]:
    prefix = test_list.parent
    with test_list.open("r") as f:
        lines = f.readlines()
    for line_number, line in enumerate(lines):
        line = line.strip()
        if len(line) <= 0 or line.startswith("#") or line.startswith(";") or line.startswith("//"):
            continue
        test_min_version = min_version
        test_max_version = max_version
        test_name = ""
        args = iter(line.split())
        for arg in args:
            if arg.startswith("-"):
                if arg == "--slow":
                    pass
                elif arg == "--min-version":
                    test_min_version = Version(next(args))
                elif arg == "--max-version":
                    test_max_version = Version(next(args))
                else:
                    raise Exception("%s:%d unknown option '%s'" % (test_list, line_number, arg))
            else:
                test_name += arg

        if test_name.endswith(".txt"):
            for r in _parse_webgl_tests(test_list_root, prefix / test_name, test_min_version, test_max_version):
                yield r
        else:
            yield _WebGLTest(
                (prefix / test_name).relative_to(test_list_root),
                test_min_version,
                test_max_version,
            )


def _filter_webgl_test_paths_for_suite_version(tests: Iterable[_WebGLTest], suite_version: Version) -> Generator[Path, None, None]:
    for test in tests:
        if test.min_version <= suite_version and test.max_version >= suite_version:
            if _use_verbose_output:
                print(f"Using test: {test.path} {test.min_version} <= {suite_version} <= {test.max_version}")
            yield test.path
        else:
            if _use_verbose_output:
                print(f"Ignoring test: {test.path} {test.min_version} <= {suite_version} <= {test.max_version}")


def _copy_webgl_test_files(
    source_tests_dir: Path,
    source_patterns: _IgnoredFilenameMatcher,
    target_dir: Path,
):
    target_test_files_dir = target_dir / "resources" / "webgl_test_files"
    if _clean_target_dirs:
        shutil.rmtree(target_test_files_dir)
    _copy_tree(source_tests_dir, target_test_files_dir, source_patterns)


def _generate_webkit_webgl_tests(
    source_tests_dir: Path,
    suite_version: Version,
    use_webgl2_context: bool,
    target_dir: Path,
    target_version_name: str,
):
    target_test_files_dir = target_dir / "resources" / "webgl_test_files"

    source_js_test_harness = _tool_dir / "webkit-webgl-test-harness-template.js"
    target_js_test_harness = target_dir / "resources" / "webkit-webgl-test-harness.js"
    target_js_test_harness.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source_js_test_harness, target_js_test_harness)
    source_js_test_harness = _tool_dir / "js-test-pre-template.js"
    target_js_test_harness = target_dir / "resources" / "js-test-pre.js"
    shutil.copyfile(source_js_test_harness, target_js_test_harness)

    test_template = (_tool_dir / "webgl-test-driver-template.html").read_text()
    expectation_template = (_tool_dir / "webgl-expectation-template.txt").read_text()

    tests = _parse_webgl_tests(source_tests_dir, (source_tests_dir / "00_test_list.txt"), suite_version, suite_version)

    target_tests_dir = target_dir / target_version_name
    if _clean_target_dirs:
        shutil.rmtree(target_tests_dir)

    for test_path in _filter_webgl_test_paths_for_suite_version(tests, suite_version):
        target_test = target_tests_dir / test_path
        target_test_impl = target_test_files_dir / test_path
        if not target_test_impl.exists():
            if _use_verbose_output:
                print(f"Ignoring test: {test_path}, implementation does not exist at {target_test_impl}")
            continue
        test_to_impl = os.path.relpath(target_test_impl, target_test.parent)
        test_link_url = f"{test_to_impl}?webglVersion=2" if use_webgl2_context else test_to_impl
        test_iframe_url = f"{test_to_impl}?quiet=1&webglVersion=2" if use_webgl2_context else f"{test_to_impl}?quiet=1"
        subs = {"test_link_url": test_link_url, "test_iframe_url": test_iframe_url, "test_name": test_path.name, "webgl_top_level_url": os.path.relpath(target_dir, target_test.parent)}
        target_test.parent.mkdir(parents=True, exist_ok=True)
        target_test.write_text(test_template % subs)
        expectation = target_test.parent / f"{target_test.stem}-expected.txt"
        expectation.write_text(expectation_template % subs)


def _find_expectations_for_removed_tests(layout_tests_dir: Path):
    test_prefixes = set()
    expectations = set()
    for root, _, files in os.walk(layout_tests_dir):
        for file_name in files:
            relative_path = (Path(root) / file_name).relative_to(layout_tests_dir)
            if relative_path.stem.endswith("-expected"):
                expectations.add(relative_path)
            else:
                test_prefixes.add(relative_path.parent / relative_path.stem)
    old_expectations = set([])
    for expectation in expectations:
        test_path = expectation.parent
        while len(test_path.parts) >= 1:
            test_prefix = test_path / expectation.stem[: -len("-expected")]
            if test_prefix in test_prefixes:
                break
            test_path = Path("/".join(test_path.parts[1:]))
        if len(test_path.parts) < 1:
            old_expectations.add(expectation)
    return old_expectations


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("-v", "--verbose", action="store_true", help="print verbose output.")
    parser.add_argument("-c", "--clean-target-dirs", action="store_true", help="remove all files from target subdirectories before updating them.")
    parser.add_argument("webgl_repository", help="path to WebGL conformance test repository root.")
    parser.add_argument(
        "-l",
        "--layout-tests-dir",
        help="base directory for searching for platform specific tests. Defaults to LayoutTests.",
        default=_layout_tests_dir,
    )

    options = parser.parse_args()
    global _use_verbose_output
    _use_verbose_output = options.verbose or False
    global _clean_target_dirs
    _clean_target_dirs = options.clean_target_dirs or False
    source_dir = Path(options.webgl_repository)
    target_dir = Path(options.layout_tests_dir) / "webgl"

    _copy_webgl_test_files(source_tests_dir=source_dir / "sdk" / "tests", source_patterns=_file_patterns, target_dir=target_dir)
    _generate_webkit_webgl_tests(source_tests_dir=source_dir / "sdk" / "tests", suite_version=Version("1.0.4"), use_webgl2_context=False, target_dir=target_dir, target_version_name="1.0.x")
    _generate_webkit_webgl_tests(source_tests_dir=source_dir / "sdk" / "tests", suite_version=Version("2.0.1"), use_webgl2_context=True, target_dir=target_dir, target_version_name="2.0.y")

    if _use_verbose_output:
        print("Updating http tests")
    http_target_dir = Path(options.layout_tests_dir) / "http" / "tests" / "webgl"
    _copy_webgl_test_files(source_tests_dir=source_dir / "sdk" / "tests", source_patterns=_http_file_patterns, target_dir=http_target_dir)
    _generate_webkit_webgl_tests(source_tests_dir=source_dir / "sdk" / "tests", suite_version=Version("1.0.4"), use_webgl2_context=False, target_dir=http_target_dir, target_version_name="1.0.x")
    _generate_webkit_webgl_tests(source_tests_dir=source_dir / "sdk" / "tests", suite_version=Version("2.0.1"), use_webgl2_context=True, target_dir=http_target_dir, target_version_name="2.0.y")

    layout_tests_dir = Path(options.layout_tests_dir)
    old_expectations = _find_expectations_for_removed_tests(layout_tests_dir)
    old_expectations = [e for e in old_expectations if "webgl" in str(e)]
    if old_expectations:
        print("Old expectations maybe found. Maybe do:\n    git rm " + " ".join(sorted([str((layout_tests_dir / e).relative_to(layout_tests_dir.parent)) for e in old_expectations])))


if __name__ == "__main__":
    sys.exit(main())
