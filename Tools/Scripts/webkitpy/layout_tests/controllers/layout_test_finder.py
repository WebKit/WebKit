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
import html
import itertools
import json
import posixpath
import re
import urllib
from collections import OrderedDict

from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models.test import Test
from webkitpy.thirdparty.wpt.manifest.sourcefile import SourceFile
from webkitpy.w3c.common import TEMPLATED_TEST_HEADER

IMPORTED_WPT_DIR = "imported/w3c/web-platform-tests"
LOCAL_WPT_PATH = "http/wpt"

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

mismatch_reference_suffixes = tuple("-mismatch" + ext for ext in supported_reference_extensions)


skipped_directories = {
    ".svn",
    "_svn",
    "_venv3",
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

# Searching with these is faster than with `in`, as the regex engine finds their first byte with memchr.
variant_re = re.compile(rb"variant")
match_re = re.compile(rb"match")
testharness_re = re.compile(rb"/resources/testharness\.js")

# A <link> start tag and its attributes, split the way the HTML tokenizer splits them.
_link_attribute = (
    rb"[^\t\n\f\r />][^\t\n\f\r />=]*"
    rb"(?:[\t\n\f\r ]*=[\t\n\f\r ]*(?:\"[^\"]*\"|'[^']*'|[^\t\n\f\r >]*))?"
)
link_tag_re = re.compile(
    rb"<(?:[A-Za-z][^\t\n\f\r />:]*:)?link(?=[\t\n\f\r />])((?:[\t\n\f\r /]*" + _link_attribute + rb")*)",
    re.IGNORECASE,
)
link_attribute_re = re.compile(
    rb"([^\t\n\f\r />][^\t\n\f\r />=]*)"
    rb"(?:[\t\n\f\r ]*=[\t\n\f\r ]*(?:\"([^\"]*)\"|'([^']*)'|([^\t\n\f\r >]*)))?"
)
reference_link_types = {b"match": "==", b"mismatch": "!="}
# The attributes of most reference <link>s, which needn't be split one by one.
common_reference_link_re = re.compile(
    rb'[\t\n\f\r ]+(?:rel="(match|mismatch)"[\t\n\f\r ]+href="([^"]*)"|href="([^"]*)"[\t\n\f\r ]+rel="(match|mismatch)")\Z'
)


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

        self.wpt_dir_prefix = fs.join(layout_tests_base_dir, *IMPORTED_WPT_DIR.split("/"), "")
        self._directory_contents = {}

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
                            self.fs.join(self.layout_tests_base_dir, dirname),
                            recursive=True,
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

        # FIXME: Skip files like `.DS_Store`
        it = self.fs.scandir(current_layout_tests_path)
        try:
            for entry in it:
                if entry.is_dir():
                    dirs.add(entry.name)
                elif entry.is_file():
                    if self.is_test_file(path, entry.name):
                        test_files.append(entry.name)
                    else:
                        non_test_files[current_layout_tests_path].add(entry.name)
        finally:
            it.close()

        # Most references are in their test's directory, so _reference_exists needn't list it again.
        if (current_layout_tests_path + self.fs.sep).startswith(self.wpt_dir_prefix):
            self._directory_contents[current_layout_tests_path] = non_test_files[current_layout_tests_path].union(test_files)

        for search_path in reversed(current_search_paths):
            if search_path is None or not self.fs.isdir(search_path):
                continue

            non_test_files[search_path] = set()
            it = self.fs.scandir(search_path)
            try:
                for entry in it:
                    if entry.is_file():
                        non_test_files[search_path].add(entry.name)
            finally:
                it.close()

        dirs -= skipped_directories
        dirs -= self.w3c_support_dirs.get(path, set())

        # The (specificity, directory) each expectation is in: the most specific directory with
        # a file of that name, as non_test_files is ordered from the least specific.
        expectation_dirs = {}
        if test_files:
            for specificity, (dirname, basenames) in enumerate(non_test_files.items()):
                expectation_dirs.update(dict.fromkeys(basenames, (specificity, dirname)))

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
                            expectation_dirs,
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
        self, dirname, basename, variants, expectation_dirs
    ):
        """Find tests for a given (dirname, basename)

        :param str dirname: dirname of the test
        :param str basename: basename of the test
        :param Optional[List[str]] variants: a list of variants to create Test objects
            for, or None in which case all known variants are run
        :param Dict[str, Tuple[int, str]] expectation_dirs: for each basename of a non-test
            file in dirname or its baseline search paths, the specificity and path of the
            most specific of those directories it is in, where dirname is least specific
        :return Iterable[Test]: an iterator over Test objects
        """
        path = self.fs.join(self.layout_tests_base_dir, dirname, basename)

        trimmed_path = self._canonicalize_test_path(path)
        if not self.fs.isabs(trimmed_path):
            trimmed_path = trimmed_path.replace(self.fs.sep, "/")

        is_imported_wpt_test = IMPORTED_WPT_DIR + "/" in trimmed_path
        is_wpt_test = is_imported_wpt_test or LOCAL_WPT_PATH + "/" in trimmed_path
        is_wpt_crash_test = self.is_wpt_crash_test(trimmed_path)

        contents = None
        if variants is not None:
            variants = [self._percent_encoded_variant(v) for v in variants]
        elif "web-platform-tests" in path:
            contents = self.fs.read_binary_file(path)
            variants = self._find_variants(contents)
        else:
            variants = [""]

        assert len(variants) >= 1

        reference_base = TestResultWriter.expected_filename(basename, self.fs, suffix="")
        assert reference_base[-1] == "."
        reference_base = reference_base[:-1]

        # Imported WPT reftests name their references with <link>s, which take precedence over
        # -expected references unless a <link>ed reference wasn't imported.
        link_references = None
        if is_imported_wpt_test and not is_wpt_crash_test:
            if contents is None:
                contents = self.fs.read_binary_file(path)
            link_references = self._link_references(trimmed_path, contents)
            if (
                link_references
                and self._has_expected_references(expectation_dirs, reference_base)
                and not all(self._reference_exists(ref_path) for _, ref_path in link_references)
            ):
                link_references = None

        for variant in variants:
            (
                expected_text_path,
                expected_image_path,
                expected_audio_path,
                reference_files,
            ) = self._expectations_for_test(
                basename, variant, expectation_dirs, reference_base, find_references=not link_references
            )

            if link_references:
                reference_files = [
                    (expectation, ref_path if ref_path == "about:blank" else ref_path + variant)
                    for expectation, ref_path in link_references
                ]

            assert expected_text_path is None or self.fs.isabs(expected_text_path)
            assert expected_image_path is None or self.fs.isabs(expected_image_path)
            assert expected_audio_path is None or self.fs.isabs(expected_audio_path)
            assert reference_files is None or all(
                ref_path == "about:blank" or self.fs.isabs(ref_path)
                for (_, ref_path) in reference_files
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
                is_wpt_test=is_wpt_test,
                is_wpt_crash_test=is_wpt_crash_test,
            )

    def _find_variants(self, contents):
        """Find variants in the contents of a web-platform-tests test"""
        # This shouldn't exist, we should be reading the WPT manifest instead.
        if not variant_re.search(contents):
            return [""]

        try:
            lines = contents.decode("utf-8").splitlines(True)
            if not lines:
                return [""]

            variants = []

            if lines[0].strip() == TEMPLATED_TEST_HEADER:
                for line in lines[1:]:
                    results = re.match(r"<!--\s*META:\s*variant=(\S*)\s*-->", line)
                    if not results:
                        continue
                    variants.append(results.group(1))
            else:
                for line in lines[1:]:
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

    def _expectations_for_test(self, basename, variant, expectation_dirs, reference_base, find_references=True):
        """Given a test basename, find expectations in expectation_dirs

        :param str reference_base: the expected filename for basename, without its extension
        :param bool find_references: whether to find -expected references
        """

        if variant:
            expected_without_ext = TestResultWriter.expected_filename(
                basename + variant, self.fs, suffix=""
            )
            assert expected_without_ext[-1] == "."
            expected_without_ext = expected_without_ext[:-1]
        else:
            expected_without_ext = reference_base

        expected_paths = []
        for extension in (".txt", ".webarchive", ".png", ".wav"):
            name = expected_without_ext + extension
            found = expectation_dirs.get(name)
            expected_paths.append(self.fs.join(found[1], name) if found else None)
        expected_text_path, expected_webarchive_path, expected_image_path, expected_audio_path = expected_paths

        reference_files = None
        if find_references and self._has_expected_references(expectation_dirs, reference_base):
            matches = tuple(map(reference_base.__add__, supported_reference_extensions))
            mismatches = tuple(map(reference_base.__add__, mismatch_reference_suffixes))
            keys = expectation_dirs.keys()

            # The references are those in the most specific directory with any. For historic
            # reasons, we return matches first, and we sort them by the filename of the match.
            # This is significant when we currently only run the first reference
            # (https://bugs.webkit.org/show_bug.cgi?id=270794).
            found = max(expectation_dirs[name] for name in matches + mismatches if name in keys)
            reference_files = [
                ("==", self.fs.join(found[1], name + variant)) for name in matches if expectation_dirs.get(name) == found
            ] + [("!=", self.fs.join(found[1], name + variant)) for name in mismatches if expectation_dirs.get(name) == found]

        return (
            expected_text_path or expected_webarchive_path,
            expected_image_path,
            expected_audio_path,
            reference_files,
        )

    @staticmethod
    def _has_expected_references(expectation_dirs, reference_base):
        keys = expectation_dirs.keys()
        return not keys.isdisjoint(map(reference_base.__add__, supported_reference_extensions)) or not keys.isdisjoint(
            map(reference_base.__add__, mismatch_reference_suffixes)
        )

    def _link_references(self, test_path, contents):
        """Find the references named by an imported WPT reftest's <link rel=match> and <link rel=mismatch>.

        This finds the same references as the WPT manifest (tools/manifest/sourcefile.py) without
        parsing the whole document, as it runs on every imported WPT test.

        :param str test_path: the path of the test, relative to layout_tests_base_dir
        :param bytes contents: the contents of the test
        :return Optional[List[Tuple[str, str]]]: (expectation, path) pairs, matches first, where
            path is absolute (with any query and fragment) or "about:blank"
        """
        if contents.startswith((b"\xff\xfe", b"\xfe\xff")):
            contents = contents.decode("utf-16", "replace").encode("utf-8")

        # Most tests don't contain "match", and "mismatch" contains it too.
        first_match = match_re.search(contents)
        if not first_match:
            return None

        # WPT considers a test with <script src="/resources/testharness.js"> to be a testharness test, even with references.
        if b"/resources/testharness.js" in contents:
            for script in testharness_re.finditer(contents):
                if contents[script.start() - 1:script.start()] in (b'"', b"'", b"="):
                    return None

        test_url = "/" + test_path.split(IMPORTED_WPT_DIR + "/", 1)[1]

        # Only <link>s containing "match" can be references, so look for those around each "match",
        # unless one isn't within a <link> starting at the closest "<", when every <link> is checked.
        links = []
        for occurrence in match_re.finditer(contents, first_match.start()):
            start = contents.rfind(b"<", 0, occurrence.start())
            link = link_tag_re.match(contents, start) if start != -1 else None
            if not link or link.end() <= occurrence.start():
                links = [link for link in link_tag_re.finditer(contents) if b"match" in link.group(1)]
                break
            if not links or links[-1].start() != start:
                links.append(link)

        matches = []
        mismatches = []
        for link in links:
            attributes = link.group(1)
            common = common_reference_link_re.match(attributes)
            if common:
                rel = common.group(1) or common.group(4)
                href = common.group(2) if common.group(1) else common.group(3)
            else:
                rel = href = None
                for name, double_quoted, single_quoted, unquoted in link_attribute_re.findall(attributes):
                    # Attribute names are case-insensitive, and the first of each name wins.
                    name = name.lower()
                    if name == b"rel":
                        if rel is None:
                            rel = double_quoted or single_quoted or unquoted
                    elif name == b"href":
                        if href is None:
                            href = double_quoted or single_quoted or unquoted

            if href is None or rel not in reference_link_types:
                continue
            comment_start = contents.rfind(b"<!--", 0, link.start())
            if comment_start != -1 and self._is_in_comment(contents, comment_start, link.start()):
                continue

            href = href.decode("utf-8", "replace")
            if "&" in href:
                href = html.unescape(href)
            ref_path = self._reference_path(test_url, href.strip("\t\n\f\r "))
            if ref_path is not None:
                (matches if rel == b"match" else mismatches).append((reference_link_types[rel], ref_path))

        if not matches and not mismatches:
            return None

        return matches + mismatches

    def _reference_exists(self, ref_path):
        if ref_path == "about:blank":
            return True
        dirname, basename = ref_path.split("?", 1)[0].split("#", 1)[0].rsplit(self.fs.sep, 1)
        # References share few directories, so list each once rather than checking each reference.
        names = self._directory_contents.get(dirname)
        if names is None:
            try:
                names = set(self.fs.listdir(dirname))
            except OSError:
                names = set()
            self._directory_contents[dirname] = names
        return basename in names

    @staticmethod
    def _is_in_comment(contents, start, index):
        """Whether index is in the comment starting at start, the last "<!--" before index."""
        start += 4
        # "<!-->" and "<!--->" are complete comments; others end with "-->" or "--!>".
        if contents.startswith((b">", b"->"), start):
            return False
        return contents.find(b"-->", start, index) == -1 and contents.find(b"--!>", start, index) == -1

    def _reference_path(self, test_url, href):
        """Resolve href against test_url as the WPT manifest does, then map it to a path.

        :return Optional[str]: the absolute path (with any query and fragment) for a URL within
            WPT, "about:blank", or None for any other URL
        """
        if not href or ":" in href or href.startswith(("?", "#", "//")):
            href = urllib.parse.urljoin(test_url, href)
            if href == "about:blank":
                return href
            url = urllib.parse.urlsplit(href)
            if url.scheme or url.netloc:
                return None

        path_end = len(href)
        for delimiter in "?#":
            index = href.find(delimiter, 0, path_end)
            if index != -1:
                path_end = index

        url_path = href[:path_end]
        if not url_path.startswith("/"):
            url_path = test_url[: test_url.rfind("/") + 1] + url_path
        url_path = posixpath.normpath(url_path)
        if "%" in url_path:
            url_path = urllib.parse.unquote(url_path)

        return self.wpt_dir_prefix + url_path[1:].replace("/", self.fs.sep) + href[path_end:]

    def is_wpt_crash_test(self, name):
        # This shouldn't exist, we should be reading the WPT manifest instead.
        # Crash tests are named with a "-crash" flag or in a "crashtests" directory.
        if "crash" not in name:
            return False

        if IMPORTED_WPT_DIR + "/" in name:
            wpt_dir = IMPORTED_WPT_DIR
            url_base = "/"
        elif LOCAL_WPT_PATH + "/" in name:
            wpt_dir = LOCAL_WPT_PATH
            url_base = "/WebKit/"
        else:
            return False

        base_dir = self.fs.join(self.layout_tests_base_dir, *wpt_dir.split('/'))
        sourcefile = SourceFile(base_dir, name.split(wpt_dir + "/", 1)[1], url_base)

        return sourcefile.name_is_crashtest
