# Copyright (C) 2021, 2022, 2023, 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import fnmatch
import itertools
import json
import re
import urllib
from collections import OrderedDict

from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models.test import Test
from webkitpy.w3c.common import TEMPLATED_TEST_HEADER

supported_test_extensions = (
    ".htm",
    ".html",
    ".mht",
    ".php",
    ".pl",
    ".py",
    ".shtml",
    ".svg",
    ".xht",
    ".xhtml",
    ".xml",
)
assert len(supported_test_extensions) == len(set(supported_test_extensions))
assert list(supported_test_extensions) == sorted(supported_test_extensions)

supported_reference_extensions = (
    ".htm",
    ".html",
    ".svg",
    ".xht",
    ".xhtml",
    ".xml",
)
assert len(supported_reference_extensions) == len(set(supported_reference_extensions))
assert list(supported_reference_extensions) == sorted(supported_reference_extensions)
assert set(supported_reference_extensions) < set(supported_test_extensions)


skipped_directories = {
    ".svn",
    "_svn",
    "resources",
    "support",
    "script-tests",
    "tools",
    "reference",
    "reftest",
}

# Files ending in -ref/-notref aren't considered expectations, but are skipped.
skipped_test_suffixes = tuple(
    "".join(pair)
    for pair in itertools.product(
        ("-expected", "-expected-mismatch", "-ref", "-notref"),
        supported_reference_extensions,
    )
)

digit_re = re.compile("([0-9]+)")


def natsort(string_to_split):
    """Returns a sort key to provide a strict total ordering of strings."""
    split = digit_re.split(string_to_split)
    # We need to return a tuple so that natsort("01") != natsort("1"), so that
    # we actually have a strict total ordering.
    split[1::2] = [(int(i), i) for i in split[1::2]]
    return split


class LayoutTestFinder(object):
    def __init__(self, fs, layout_tests_base_dir, baseline_search_paths):
        """Find layout tests.

        :param FileSystem fs: the current filesystem object
        :param str layout_tests_base_dir: the base directory of LayoutTests
            (see: Port.layout_tests_dir())
        :param List[str] baseline_search_paths: the baseline search paths, from most to
            least specific (see: Port.baseline_search_path(device_type))
        """
        self.fs = fs

        layout_tests_base_dir = fs.normpath(fs.realpath(layout_tests_base_dir))
        baseline_search_paths = [
            fs.normpath(fs.realpath(bsp)) for bsp in baseline_search_paths
        ]

        self.layout_tests_base_dir = layout_tests_base_dir
        self.baseline_search_paths = baseline_search_paths

        self.w3c_support_dirs, self.w3c_support_files = self._load_w3c_resource_data()

    def _load_w3c_resource_data(self):
        w3c_path = self.fs.join(
            self.layout_tests_base_dir,
            "imported",
            "w3c",
        )

        if not self.fs.exists(w3c_path):
            return {}, {}

        path = self.fs.join(
            w3c_path,
            "resources",
            "resource-files.json",
        )

        with self.fs.open_binary_file_for_reading(path) as fd:
            json_data = json.load(fd)

        dirs_by_parent = {}
        for fullname in json_data["directories"]:
            dirname, basename = fullname.rsplit("/", 1)
            dirs_by_parent.setdefault(
                ("imported/w3c/" + dirname).replace("/", self.fs.sep), set()
            ).add(basename)

        files_by_parent = {}
        for fullname in json_data["files"]:
            dirname, basename = fullname.rsplit("/", 1)
            files_by_parent.setdefault(
                ("imported/w3c/" + dirname).replace("/", self.fs.sep), set()
            ).add(basename)

        return dirs_by_parent, files_by_parent

    def _canonicalize_test_path(self, path):
        base = self.layout_tests_base_dir
        fs = self.fs

        # Fast-case the common case.
        normpath = fs.normpath(path)
        if normpath.startswith(base + fs.sep):
            return normpath[len(base) + 1:]

        relpath = fs.relpath(path, base)
        if relpath.startswith(".." + fs.sep):
            realpath = fs.realpath(path)
            if realpath != path:
                return self._canonicalize_test_path(realpath)

            return fs.abspath(path)
        else:
            return relpath

    def get_tests(self, globs=None):
        """Get Test objects for those with paths specified by globs.

        :param Optional[List[str]] globs: a list of globs to get tests for; if falsy
            then "*" is used instead
        """
        # The majority of the implementation is actually delegated; this method just
        # does some basic handling of the globs before deciding which directories to
        # scan.

        if not globs:
            # We can't just call _get_tests_in_directory, because to match legacy
            # behavior we use "*" here, which results in different ordering (see below).
            globs = ["*"]

        all_split_globs = itertools.chain.from_iterable(
            self._split_glob(glob) for glob in globs
        )

        for dirname, group in itertools.groupby(all_split_globs, key=lambda x: x[0]):
            if dirname:
                dirs = sorted(
                    {
                        self._canonicalize_test_path(p)
                        for p in self.fs.glob(
                            self.fs.join(self.layout_tests_base_dir, dirname)
                        )
                        if self.fs.isdir(p)
                    },
                    key=natsort,
                )
            else:
                dirs = [""]

            fnfilter = [(basename or "*", variant) for _, basename, variant in group]

            for d in dirs:
                # We intermix tests and directories together from the glob, rather than
                # (as we do when recursing) giving all tests in a directory together.
                for item in self._process_directory(
                    d,
                    fnfilter=fnfilter,
                ):
                    if isinstance(item, Test):
                        yield item
                    else:
                        for test in self._get_tests_in_directory(
                            directory=self.fs.join(d, item)
                        ):
                            yield test

    def _split_glob(self, glob):
        """Split a glob string into possible (dirname, basename, variant) 3-tuples.

        This deals with the ambiguity inherent in passing a name like foo?bar: is this
        mean to match fooXbar, or is it the ?bar variant of the foo test.
        """
        # Ignore everything after a # (this is always a variant).
        if "#" in glob:
            name, variant = glob.rsplit("#", 1)
            variant = "#" + variant
        else:
            name, variant = glob, ""

        # Split based on ?, as we need to consider this as either a glob string
        # or a query string.
        splits = name.split("?")
        for i in range(1, len(splits) + 1):
            dirname, basename = self.fs.split("?".join(splits[:i]))

            if basename and len(splits) > i:
                # If we have more splits, we have a possible variant.
                yield (dirname, basename, "?" + "?".join(splits[i:]) + variant)
            elif basename or not variant and len(splits) == i:
                # If we have a basename or no variants.
                yield (dirname, basename, variant)

    def _get_tests_in_directory(self, directory):
        """Get all tests in directory, recursively."""
        stack = [
            (
                directory,
                [
                    self.fs.join(search_path, directory)
                    for search_path in self.baseline_search_paths
                ],
            )
        ]

        while stack:
            current_path, current_search_paths = stack.pop()

            next_paths = []
            for item in self._process_directory(
                current_path,
                current_search_paths=current_search_paths,
            ):
                if isinstance(item, Test):
                    yield item
                else:
                    next_paths.append(item)

            for d in reversed(next_paths):
                next_search_paths = [
                    self.fs.join(cur, d) if cur is not None else None
                    for cur in current_search_paths
                ]
                assert len(next_search_paths) == len(self.baseline_search_paths)
                stack.append((self.fs.join(current_path, d), next_search_paths))

    def _process_directory(
        self,
        path,
        fnfilter=None,
        current_search_paths=None,
    ):
        """Process a directory, optionally filtering names within it.

        Names in the directory are looped over once per item in fnfilter (or once, if no
        filter is provided), so if a test name or directory is matched by multiple items
        in fnfilter it will appear multiple times in the returned list.

        :param str path: Path to the directory, relative to layout_tests_base_dir
        :param Optional[List[Tuple[str, str]]] fnfilter: Any fnfilter to apply, along
            with any variants for that filter.
        :param Optional[List[Optional[str]]] current_search_paths: A list of baseline
            search paths for path or (optionally) None if the directory doesn't exist
        :return List[Union[Test, str]]: Found tests and directories to recurse into.

        """
        if fnfilter is None:
            fnfilter = [("*", "")]

        if current_search_paths is None:
            current_search_paths = [
                self.fs.join(search_path, path)
                for search_path in self.baseline_search_paths
            ]

        assert len(current_search_paths) == len(self.baseline_search_paths)

        test_files = []
        non_test_files = OrderedDict()
        dirs = set()

        current_layout_tests_path = self.fs.join(self.layout_tests_base_dir, path)
        non_test_files[current_layout_tests_path] = set()

        it = self.fs.scandir(current_layout_tests_path)
        try:
            for entry in it:
                if entry.is_dir(follow_symlinks=False):
                    dirs.add(entry.name)
                elif entry.is_file(follow_symlinks=False):
                    if self.is_test_file(path, entry.name):
                        test_files.append(entry.name)
                    else:
                        non_test_files[current_layout_tests_path].add(entry.name)
        finally:
            it.close()

        for search_path in reversed(current_search_paths):
            if search_path is None or not self.fs.isdir(search_path):
                continue

            non_test_files[search_path] = set()
            it = self.fs.scandir(search_path)
            try:
                for entry in it:
                    if entry.is_file(follow_symlinks=False):
                        non_test_files[search_path].add(entry.name)
            finally:
                it.close()

        dirs -= skipped_directories
        dirs -= self.w3c_support_dirs.get(path, set())

        merged_items = sorted(test_files + [d + "/" for d in dirs], key=natsort)
        found = []

        for pattern, variant in fnfilter:
            compiled = re.compile(fnmatch.translate(pattern))
            for f in merged_items:
                if f[-1] == "/":
                    d = f[:-1]
                    if pattern == "*" or compiled.match(d):
                        found.append(d)
                elif pattern == "*" or compiled.match(f):
                    found.extend(
                        self._tests_for_path(
                            path,
                            f,
                            [variant] if variant else None,
                            non_test_files,
                        )
                    )

        return found

    def is_test_file(self, dirname, basename):
        if (
            not basename.endswith(supported_test_extensions)
            or basename.startswith(("ref-", "notref-"))
            or basename.endswith(skipped_test_suffixes)
            or basename.endswith("_wsh.py")
            or basename in ("boot.xml", "root.xml")
            or dirname.startswith(self.fs.join("imported", "w3c", ""))
            and (
                basename.endswith(".py")
                or basename in self.w3c_support_files.get(dirname, set())
            )
        ):
            return False
        return True

    def _tests_for_path(
        self, dirname, basename, variants, non_test_files_by_search_path
    ):
        """Find tests for a given (dirname, basename)

        :param str dirname: dirname of the test
        :param str basename: basename of the test
        :param Optional[List[str]] variants: a list of variants to create Test objects
            for, or None in which case all known variants are run
        :param OrderedDict[str, Set[str]] non_test_files_by_search_path: an OrderedDict,
            whose key is dirnames, starting with the current dirname above followed by
            baseline search paths in increasing specificity, and whose value is sets of
            basenames that exist in that dirname
        :return Iterable[Test]: an iterator over Test objects
        """
        path = self.fs.join(self.layout_tests_base_dir, dirname, basename)

        if variants is None:
            variants = self._find_variants(path)
        else:
            variants = [self._percent_encoded_variant(v) for v in variants]

        assert len(variants) >= 1

        for variant in variants:
            trimmed_path = self._canonicalize_test_path(path)
            if not self.fs.isabs(trimmed_path):
                trimmed_path = trimmed_path.replace(self.fs.sep, "/")

            (
                expected_text_path,
                expected_image_path,
                expected_audio_path,
                reference_files,
            ) = self._expectations_for_test(
                basename + variant, non_test_files_by_search_path
            )

            assert expected_text_path is None or self.fs.isabs(expected_text_path)
            assert expected_image_path is None or self.fs.isabs(expected_image_path)
            assert expected_audio_path is None or self.fs.isabs(expected_audio_path)
            assert reference_files is None or all(
                self.fs.isabs(ref_path) for (_, ref_path) in reference_files
            )

            yield Test(
                test_path=trimmed_path + variant,
                expected_text_path=expected_text_path,
                expected_audio_path=expected_audio_path,
                expected_image_path=expected_image_path,
                reference_files=(
                    tuple(reference_files) if reference_files is not None else None
                ),
                is_http_test="http/test" in trimmed_path,
                is_websocket_test=(
                    "websocket/" in trimmed_path
                    or "http/test" in trimmed_path
                    and "websocket" in trimmed_path + variant
                ),
                is_wpt_test=(
                    "imported/w3c/web-platform-tests/" in trimmed_path
                    or "http/wpt/" in trimmed_path
                ),
                is_wpt_crash_test=self.is_wpt_crash_test(trimmed_path),
            )

    def _find_variants(self, f):
        """Find variants for path `f`"""
        # This shouldn't exist, we should be reading the WPT manifest instead.
        if "web-platform-tests" not in f:
            return [""]

        opened_file = self.fs.open_text_file_for_reading(f)
        try:
            first_line = opened_file.readline()
            if not first_line:
                return [""]

            variants = []

            if first_line.strip() == TEMPLATED_TEST_HEADER:
                for line in iter(opened_file.readline, ""):
                    results = re.match(r"<!--\s*META:\s*variant=(\S*)\s*-->", line)
                    if not results:
                        continue
                    variants.append(results.group(1))
            else:
                for line in iter(opened_file.readline, ""):
                    try:
                        line = line.lstrip()
                        if not line.startswith("<meta"):
                            continue
                        if not re.search(r"name=['\"]?variant['\"]?", line):
                            continue
                        start_index = line.find("content=")
                        if start_index < 0:
                            continue
                        start_index += 8
                        end_chars = ()
                        if line[start_index] == '"' or line[start_index] == "'":
                            end_chars = (line[start_index],)
                            start_index += 1
                        else:
                            end_chars = (" ", ">")
                        end_index = start_index
                        while line[end_index] not in end_chars:
                            end_index += 1
                        variants.append(line[start_index:end_index])
                    except IndexError:
                        continue

            variants = [
                self._percent_encoded_variant(v)
                for v in variants
                if self._is_valid_variant(v)
            ]
        except UnicodeDecodeError:
            return [""]

        if not variants:
            return [""]

        return variants

    def _percent_encoded_variant(self, variant):
        m = re.search(
            "^(?P<path>[^?#]*)(?P<variant>(?P<query>\\?[^#]*)?(?P<fragment>#.*)?)$",
            variant,
        )
        path, _, query, fragment = m.groups()
        assert m.group("path") == ""

        # This is all code points not in the "query percent-encode set" [URL], minus
        # characters urllib.parse.quote never quotes.
        safe_query = "!$%&'()*+,/:;=?@[\\]^`{|}~"

        # This is all code points not in the "fragment percent-encode set" [URL], minus
        # characters urllib.parse.quote never quotes.
        safe_fragment = "!#$%&'()*+,/:;=?@[\\]^{|}~"

        query = "" if query is None else query
        fragment = "" if fragment is None else fragment

        query = urllib.parse.quote(query, safe=safe_query, encoding="utf-8")
        fragment = urllib.parse.quote(
            fragment, safe=safe_fragment, encoding="utf-8"
        )

        return "{}{}".format(query, fragment)

    def _is_valid_variant(self, variant):
        """Check whether a given variant is valid"""
        return variant == "" or (
            len(variant) > 1 and variant[0] in ("?", "#") and variant != "?#"
        )

    def _expectations_for_test(self, name, non_test_files_by_search_path):
        """Given a test basename, find expectations in non_test_files_by_search_path"""

        expected_without_ext = TestResultWriter.expected_filename(
            name, self.fs, suffix=""
        )
        assert expected_without_ext[-1] == "."
        expected_without_ext = expected_without_ext[:-1]

        expected_text_path = None
        expected_webarchive_path = None
        expected_image_path = None
        expected_audio_path = None
        reference_files = None

        txt_name = expected_without_ext + ".txt"
        webarchive_name = expected_without_ext + ".webarchive"
        png_name = expected_without_ext + ".png"
        wav_name = expected_without_ext + ".wav"

        match_reference_names = {
            expected_without_ext + ext for ext in supported_reference_extensions
        }

        mismatch_reference_names = {
            "".join((expected_without_ext, "-mismatch", ext))
            for ext in supported_reference_extensions
        }

        for dirname, basename_set in reversed(non_test_files_by_search_path.items()):
            if expected_text_path is None and txt_name in basename_set:
                expected_text_path = self.fs.join(dirname, txt_name)

            if expected_webarchive_path is None and webarchive_name in basename_set:
                expected_webarchive_path = self.fs.join(dirname, webarchive_name)

            if expected_image_path is None and png_name in basename_set:
                expected_image_path = self.fs.join(dirname, png_name)

            if expected_audio_path is None and wav_name in basename_set:
                expected_audio_path = self.fs.join(dirname, wav_name)

            if reference_files is None:
                matches = basename_set & match_reference_names
                mismatches = basename_set & mismatch_reference_names
                if matches or mismatches:
                    # For historic reasons, we return matches first, and we sort them by
                    # the filename of the match. This is significant when we currently
                    # only run the first reference
                    # (https://bugs.webkit.org/show_bug.cgi?id=270794).
                    reference_files = [
                        ("==", self.fs.join(dirname, m)) for m in sorted(matches)
                    ] + [("!=", self.fs.join(dirname, m)) for m in sorted(mismatches)]

        return (
            expected_text_path or expected_webarchive_path,
            expected_image_path,
            expected_audio_path,
            reference_files,
        )

    def is_wpt_crash_test(self, name):
        # This shouldn't exist, we should be reading the WPT manifest instead.
        if "imported/w3c/web-platform-tests/" not in name and "http/wpt/" not in name:
            return False

        filename, _ = self.fs.splitext(name)
        return (
            filename.endswith(("-crash", "-crash.tentative")) or "/crashtests/" in name
        )
