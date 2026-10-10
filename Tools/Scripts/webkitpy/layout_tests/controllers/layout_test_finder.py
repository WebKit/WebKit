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
import logging
import posixpath
import re
import urllib.parse
from collections import OrderedDict

from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models.server_routing import ServerType
from webkitpy.layout_tests.models.test import Reference, Test, test_name_and_variant
from webkitpy.thirdparty.wpt.manifest.sourcefile import SourceFile

_log = logging.getLogger(__name__)

IMPORTED_WPT_DIR = "imported/w3c/web-platform-tests"

supported_test_extensions = (
    ".any.js",
    ".htm",
    ".html",
    ".mht",
    ".php",
    ".pl",
    ".py",
    ".shtml",
    ".svg",
    ".window.js",
    ".worker.js",
    ".xht",
    ".xhtml",
    ".xml",
)
assert len(supported_test_extensions) == len(set(supported_test_extensions))
assert list(supported_test_extensions) == sorted(supported_test_extensions)

# .any.js / .worker.js / .window.js are only treated as tests under WPT roots.
wpt_only_test_extensions = (
    ".any.js",
    ".window.js",
    ".worker.js",
)
assert set(wpt_only_test_extensions) < set(supported_test_extensions)

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


def natsort(string_to_split):
    """Returns a sort key to provide a strict total ordering of strings."""
    split = digit_re.split(string_to_split)
    # We need to return a tuple so that natsort("01") != natsort("1"), so that
    # we actually have a strict total ordering.
    split[1::2] = [(int(i), i) for i in split[1::2]]
    return split


class LayoutTestFinder(object):
    def __init__(self, fs, layout_tests_base_dir, baseline_search_paths, test_routes=None, all_baseline_search_paths=None):
        """Find layout tests.

        :param FileSystem fs: the current filesystem object
        :param str layout_tests_base_dir: the base directory of LayoutTests
            (see: Port.layout_tests_dir())
        :param List[str] baseline_search_paths: the baseline search paths, from most to
            least specific (see: Port.baseline_search_path(device_type))
        :param Optional[List[ServerRoute]] test_routes: the server routes
            (see: Port.test_routes()); each test_file_dir is compared as a
            string against "/"-separated test names
        :param Optional[List[str]] all_baseline_search_paths: the full set of baseline
            search paths across all platforms (see: Port.all_baseline_search_paths()).
            If None, defaults to baseline_search_paths. Used by all_baselines_for_test().
        """
        self.fs = fs

        layout_tests_base_dir = fs.normpath(fs.realpath(layout_tests_base_dir))
        baseline_search_paths = [
            fs.normpath(fs.realpath(bsp)) for bsp in baseline_search_paths
        ]
        all_baseline_search_paths = all_baseline_search_paths or baseline_search_paths
        self.all_baseline_search_paths = [
            fs.normpath(fs.realpath(bsp)) for bsp in all_baseline_search_paths
        ]

        self.layout_tests_base_dir = layout_tests_base_dir
        self.baseline_search_paths = baseline_search_paths
        self.test_routes = test_routes or []

        # WPT routes, sorted by directory-depth specificity (most specific first)
        # so that deeper directories are preferred over shallower ones.
        self._wpt_routes = sorted(
            (route for route in self.test_routes
             if route.server_type & ServerType.WPT),
            key=lambda route: route.test_file_dir.count("/"),
            reverse=True,
        )

        self.w3c_support_dirs, self.w3c_support_files = self._load_w3c_resource_data()

    def baselines_for_test(self, test_name, suffix):
        """Return baseline files for test_name using baseline_search_paths (current platform).

        Like Port.expected_baselines(test_name, suffix, all_baselines=False) — returns only
        the first matching baseline found, then stops.

        :param str test_name: relative test path (may include variant)
        :param str suffix: file suffix including dot (e.g. '.txt', '.png')
        :returns: list of (platform_dir, baseline_filename) pairs. platform_dir is
            None if no file was found anywhere.
        """
        assert suffix.startswith('.')
        baseline_filename = TestResultWriter.expected_filename(test_name, self.fs, suffix=suffix[1:])

        search_paths = list(self.baseline_search_paths) + [self.layout_tests_base_dir]
        for platform_dir in search_paths:
            if self.fs.exists(self.fs.join(platform_dir, baseline_filename)):
                return [(platform_dir, baseline_filename)]

        return [(None, baseline_filename)]

    def all_baselines_for_test(self, test_name, suffix):
        """Return all baseline files for test_name across all_baseline_search_paths.

        Mirrors Port.expected_baselines(test_name, suffix, all_baselines=True).

        :param str test_name: relative test path (may include variant, e.g. 'dir/foo.html?var')
        :param str suffix: file suffix including dot (e.g. '.txt', '.png')
        :returns: list of (platform_dir, baseline_filename) pairs. platform_dir is
            None if no file was found anywhere (same convention as Port.expected_baselines).
        """
        assert suffix.startswith('.')
        # TestResultWriter.expected_filename handles variant sanitization internally
        baseline_filename = TestResultWriter.expected_filename(test_name, self.fs, suffix=suffix[1:])

        baselines = []
        for platform_dir in list(self.all_baseline_search_paths) + [self.layout_tests_base_dir]:
            if self.fs.exists(self.fs.join(platform_dir, baseline_filename)):
                baselines.append((platform_dir, baseline_filename))

        if not baselines:
            baselines.append((None, baseline_filename))
        return baselines

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

        merged_items = sorted(test_files + [d + "/" for d in dirs], key=natsort)
        found = []

        for pattern, variant in fnfilter:
            compiled = re.compile(fnmatch.translate(pattern))
            # Generated Test.test_path variants are always percent-encoded
            # (see _percent_encoded_variant); encode the user-supplied
            # variant once here so both match sites below compare like with
            # like, whichever branch a given file falls into.
            encoded_variant = self._percent_encoded_variant(variant) if variant else variant
            wanted_variants = [encoded_variant] if encoded_variant else None
            for f in merged_items:
                if f[-1] == "/":
                    d = f[:-1]
                    if pattern == "*" or compiled.match(d):
                        found.append(d)
                    continue
                if pattern == "*" or compiled.match(f):
                    found.extend(
                        self._tests_for_path(path, f, wanted_variants, non_test_files)
                    )
                else:
                    for t in self._tests_for_path(path, f, None, non_test_files):
                        t_file_part, t_variant = test_name_and_variant(t.test_path)
                        t_basename = posixpath.basename(t_file_part)
                        if compiled.match(t_basename) and (not variant or t_variant == encoded_variant):
                            found.append(t)

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
        # .any.js/.window.js/.worker.js are only test sources under WPT roots.
        if basename.endswith(wpt_only_test_extensions) and not self._wpt_route_for_dirname(dirname):
            return False
        return True

    def _wpt_route_for_dirname(self, dirname):
        """Return the most-specific WPT ServerRoute containing dirname, or None
        if dirname isn't under any WPT route."""
        dirname_with_sep = dirname.replace(self.fs.sep, "/")
        for route in self._wpt_routes:
            if dirname_with_sep == route.test_file_dir or dirname_with_sep.startswith(route.test_file_dir + "/"):
                return route
        return None

    def _tests_for_path(
        self, dirname, basename, variants, non_test_files_by_search_path
    ):
        """Find tests for a given (dirname, basename).

        Dispatches to the WPT-aware code path when the file is under a WPT
        route; otherwise uses the simple non-WPT path.

        :param str dirname: dirname of the test
        :param str basename: basename of the test
        :param Optional[List[str]] variants: a list of variants to create Test
            objects for, or None in which case all known variants are run
        :param OrderedDict[str, Set[str]] non_test_files_by_search_path: an
            OrderedDict, whose key is dirnames, starting with the current
            dirname above followed by baseline search paths in increasing
            specificity, and whose value is sets of basenames that exist in
            that dirname
        :return Iterable[Test]: an iterator over Test objects
        """
        if self._wpt_route_for_dirname(dirname):
            return self._wpt_tests_for_path(
                dirname, basename, variants, non_test_files_by_search_path
            )
        return self._non_wpt_tests_for_path(
            dirname, basename, variants, non_test_files_by_search_path
        )

    def _non_wpt_tests_for_path(
        self, dirname, basename, variants, non_test_files_by_search_path
    ):
        # Non-WPT tests don't have intrinsic variants; if the caller supplied
        # explicit variants (e.g. via a glob like "foo.html?abc"), produce one
        # Test per requested variant.
        path = self.fs.join(self.layout_tests_base_dir, dirname, basename)

        trimmed_path = self._canonicalize_test_path(path)
        if not self.fs.isabs(trimmed_path):
            trimmed_path = trimmed_path.replace(self.fs.sep, "/")

        if not variants:
            variants = [""]

        for variant in variants:
            (
                expected_text_path,
                expected_image_path,
                expected_audio_path,
                reference_files,
            ) = self._expectations_for_test(
                basename, variant, non_test_files_by_search_path
            )

            assert expected_text_path is None or self.fs.isabs(expected_text_path)
            assert expected_image_path is None or self.fs.isabs(expected_image_path)
            assert expected_audio_path is None or self.fs.isabs(expected_audio_path)
            assert reference_files is None or all(
                self.fs.isabs(ref.path) for ref in reference_files
            )

            served_by = ServerType.FILE
            for route in self.test_routes:
                if trimmed_path.startswith(route.test_file_dir + "/"):
                    served_by = served_by | route.server_type
                    break
            if not (served_by & ServerType.WPT) and "websocket" in trimmed_path + variant:
                served_by = served_by | ServerType.WEBSOCKET

            # Parse filename flags (e.g., test.https.html, test.h2.html, test.sub.html)
            basename_lower = basename.lower()
            detected_flags = set()
            for flag in ("https", "h2", "sub"):
                if "." + flag + "." in basename_lower:
                    detected_flags.add(flag)
            # HTTP tests in ssl/ directories are served over HTTPS
            if (served_by & ServerType.HTTP) and ("/ssl/" in "/" + trimmed_path + "/"):
                detected_flags.add("https")
            flags = frozenset(detected_flags)

            # Parse fuzzy metadata at discovery time for reftests.
            fuzzy = None
            if reference_files is not None:
                rel_path = self.fs.join(dirname, basename)
                try:
                    contents = self.fs.read_binary_file(path)
                except (IOError, OSError, UnicodeDecodeError):
                    contents = None
                try:
                    sourcefile = SourceFile(
                        self.layout_tests_base_dir, rel_path, "/", contents=contents
                    )
                    if sourcefile.fuzzy:
                        fuzzy = {}
                        for key, value in sourcefile.fuzzy.items():
                            if key is None:
                                fuzzy[None] = value
                            else:
                                # key is (test_url, ref_url, reftype); only ref_url matters
                                # url_base is "/", so ref_url is like /dir/reference.html
                                ref_url = key[1]
                                ref_rel_path = ref_url.lstrip("/")
                                ref_abs_path = self.fs.join(
                                    self.layout_tests_base_dir, *ref_rel_path.split("/")
                                )
                                fuzzy[ref_abs_path] = value
                except ValueError as e:
                    # SourceFile raises ValueError for malformed fuzzy metadata
                    # (see _wpt_tests_for_path); run the test without it.
                    _log.warning("Ignoring fuzzy metadata of %s: %s", trimmed_path, e)

            yield Test(
                test_path=trimmed_path + variant,
                file_path=trimmed_path,
                expected_text_path=expected_text_path,
                expected_audio_path=expected_audio_path,
                expected_image_path=expected_image_path,
                reference_files=(
                    tuple(reference_files) if reference_files is not None else None
                ),
                served_by=served_by,
                flags=flags,
                fuzzy=fuzzy,
            )

    def _wpt_tests_for_path(
        self, dirname, basename, variants, non_test_files_by_search_path
    ):
        """Use SourceFile.manifest_items() to enumerate WPT tests for a file."""
        route = self._wpt_route_for_dirname(dirname)
        assert route is not None
        wpt_prefix, url_base = route.test_file_dir, route.url_base

        full_path = self.fs.join(self.layout_tests_base_dir, dirname, basename)
        wpt_base_dir = self.fs.join(
            self.layout_tests_base_dir, *wpt_prefix.split("/")
        )
        rel_path = self.fs.relpath(full_path, wpt_base_dir)

        # file_path for all items: <wpt_prefix>/<rel_path> in slash form.
        rel_path_slash = rel_path.replace(self.fs.sep, "/")
        file_path = wpt_prefix + "/" + rel_path_slash

        # Use the FileSystem abstraction so we work with mock filesystems
        # used by unit tests.
        try:
            contents = self.fs.read_binary_file(full_path)
        except (IOError, OSError, UnicodeDecodeError):
            # File may have been deleted between directory enumeration and
            # parsing, or contain bytes the filesystem can't return. Fall
            # back to letting SourceFile read from disk itself.
            contents = None

        try:
            sourcefile = SourceFile(wpt_base_dir, rel_path, url_base, contents=contents)
            item_type, items = sourcefile.manifest_items()
        except ValueError as e:
            # SourceFile raises ValueError for malformed test metadata
            # (bad fuzzy values, malformed reference keys, print reftests
            # without refs, etc.). Skip such files rather than aborting
            # the entire discovery pass.
            _log.warning("Skipping WPT file %s: %s", file_path, e)
            return

        wanted_variants = None
        if variants is not None:
            wanted_variants = set(variants)

        if item_type not in ("testharness", "reftest", "crashtest"):
            # Not a runnable type for WKTR. `support` for HTML-ish files that
            # the legacy finder would have run as synthetic tests silently yields
            # nothing (warning removed — asymmetric with non-WPT silent-skip and
            # noisy under union match). Everything else (manual, wdspec, aamtest,
            # visual, conformancechecker, test262, print-reftest) silently yields
            # nothing.
            return

        for item in items:
            # Drop the multi-global jsshell variant (its URL is the .any.js
            # source, which isn't loadable in a browser). `jsshell` is set
            # per-variant on TestharnessTest from sourcefile.py's
            # `global_suffixes` — see item.py:198 and sourcefile.py:1030.
            if item_type == "testharness" and item.jsshell:
                continue

            test_path = self._wpt_url_to_test_path(item.url, wpt_prefix, url_base)
            if test_path is None:
                continue

            test_file_part, item_variant = test_name_and_variant(test_path)
            if wanted_variants is not None and item_variant not in wanted_variants:
                continue

            if test_file_part != file_path:
                # Generated variant from .any.js/.window.js/.worker.js. If an
                # on-disk stub exists AND classifies as anything other than
                # `support`, both this source and the stub will yield Test
                # objects for the same URL — a duplicate dispatch. E.g., an
                # importer-emitted stub inside /crashtests/ classifies as
                # `crashtest`, not `support`, which is what makes crashtest
                # paired stubs uniquely buggy under SourceFile-based
                # discovery. The warning falls silent once the WebKit WPT
                # importer stops emitting the paired stubs (or SourceFile
                # is fixed to classify them uniformly as `support`).
                stub_rel_slash = test_file_part[len(wpt_prefix) + 1:]
                stub_rel = stub_rel_slash.replace("/", self.fs.sep)
                stub_full_path = self.fs.join(wpt_base_dir, stub_rel)
                if self.fs.exists(stub_full_path):
                    try:
                        stub_contents = self.fs.read_binary_file(stub_full_path)
                    except (IOError, OSError, UnicodeDecodeError):
                        stub_contents = None
                    try:
                        stub_item_type, _ = SourceFile(
                            wpt_base_dir, stub_rel, url_base,
                            contents=stub_contents,
                        ).manifest_items()
                    except ValueError:
                        stub_item_type = None
                    if stub_item_type is not None and stub_item_type != "support":
                        _log.warning(
                            "Duplicate WPT Test dispatch: %s emitted from %s "
                            "(as %s) and from on-disk stub %s (as %s) — the "
                            "WebKit WPT importer should either stop emitting "
                            "the stub, or SourceFile should classify it as "
                            "`support`.",
                            test_path, file_path, item_type,
                            stub_full_path, stub_item_type,
                        )

            kwargs = dict(
                test_path=test_path,
                file_path=file_path,
                served_by=ServerType.WPT,
                flags=frozenset(item._flags),
            )

            if item_type == "testharness":
                # Testharness tests are compared against -expected.{txt,png,wav}.
                (kwargs["expected_text_path"],
                 kwargs["expected_image_path"],
                 kwargs["expected_audio_path"]) = self._baselines_for_test(
                    self._wpt_generated_basename(item.url),
                    item_variant,
                    non_test_files_by_search_path,
                )
            elif item_type == "crashtest":
                kwargs["is_crash_test"] = True
            elif item_type == "reftest":
                # Reftests compare against their reference(s), not baseline
                # files; skip the -expected.{txt,png,wav} lookup. References
                # come from the manifest; fuzzy from item.fuzzy.
                reference_files = self._wpt_references_for_item(item, wpt_prefix, url_base)
                # TEMPORARY scaffold: WebKit's WPT importer renames
                # manifest-declared refs to <stem>-expected.<ext> on import
                # AND hand-tweaks them for WebKit-specific rendering quirks,
                # so the imported sibling (when present) is the canonical
                # baseline that TestExpectations is calibrated against —
                # NOT the upstream manifest ref, even when the manifest URL
                # also resolves to a real file on disk. Prefer the imported
                # sibling; only use manifest refs when no sibling exists.
                # Delete this block once the importer is fixed (when the
                # warning stops firing across a full run).
                if reference_files:
                    _, _, _, sibling_refs = self._expectations_for_test(
                        self._wpt_generated_basename(item.url),
                        item_variant,
                        non_test_files_by_search_path,
                    )
                    if sibling_refs:
                        reference_files = sibling_refs
                    else:
                        existing = [ref for ref in reference_files if self.fs.exists(ref.path)]
                        if not existing:
                            _log.warning(
                                "%s: WPT manifest reference(s) missing on disk "
                                "and no imported -expected.<ext> sibling found: %s",
                                test_path,
                                ", ".join(ref.path for ref in reference_files),
                            )
                        reference_files = existing
                if reference_files:
                    kwargs["reference_files"] = tuple(reference_files)

                fuzzy = None
                raw_fuzzy = getattr(item, "fuzzy", None)
                if raw_fuzzy:
                    fuzzy = {}
                    # `single_test_runner._fuzzy_tolerance_for_reference` looks
                    # up tolerances by the Reference's `path` (an absolute
                    # filesystem path -- see _wpt_references_for_item) or by
                    # `None` for the default. Translate WPT's
                    # `(test_url, ref_url, reftype)` keys into that shape.
                    for key, value in raw_fuzzy.items():
                        if key is None:
                            fuzzy[None] = value
                        else:
                            ref_url = key[1]
                            ref_test_path = self._wpt_url_to_test_path(ref_url, wpt_prefix, url_base)
                            if ref_test_path is None:
                                continue
                            ref_file_part, _ = test_name_and_variant(ref_test_path)
                            ref_abs_path = self.fs.join(
                                self.layout_tests_base_dir, *ref_file_part.split("/")
                            )
                            fuzzy[ref_abs_path] = value
                if fuzzy is not None:
                    kwargs["fuzzy"] = fuzzy

            yield Test(**kwargs)

    def _percent_encoded_variant(self, variant):
        # Verbatim from origin/main:layout_test_finder.py:517.
        m = re.search(
            "^(?P<path>[^?#]*)(?P<variant>(?P<query>\\?[^#]*)?(?P<fragment>#.*)?)$",
            variant,
        )
        path, _, query, fragment = m.groups()
        assert m.group("path") == ""

        safe_query = "!$%&'()*+,/:;=?@[\\]^`{|}~"
        safe_fragment = "!#$%&'()*+,/:;=?@[\\]^{|}~"

        query = "" if query is None else query
        fragment = "" if fragment is None else fragment

        query = urllib.parse.quote(query, safe=safe_query, encoding="utf-8")
        fragment = urllib.parse.quote(fragment, safe=safe_fragment, encoding="utf-8")

        return "{}{}".format(query, fragment)

    def _wpt_url_to_test_path(self, url, wpt_prefix, url_base):
        """Convert a WPT manifest item URL into a layout-test path."""
        if not url.startswith("/"):
            return None
        if url_base == "/":
            rel = url[1:]
        else:
            assert url_base.startswith("/") and url_base.endswith("/")
            if not url.startswith(url_base):
                return None
            rel = url[len(url_base):]

        # Encode the variant (query/fragment) — `?` always precedes `#` in a
        # well-formed URL, so finding `?` first is sufficient; the helper
        # handles fragment internally.
        for sep in ("?", "#"):
            i = rel.find(sep)
            if i >= 0:
                rel = rel[:i] + self._percent_encoded_variant(rel[i:])
                break

        return wpt_prefix + "/" + rel

    def _wpt_generated_basename(self, url):
        """Return the basename of a WPT item URL with any query/fragment removed."""
        # Strip query/fragment.
        for sep in ("?", "#"):
            i = url.find(sep)
            if i >= 0:
                url = url[:i]
        return url.rsplit("/", 1)[-1]

    def _wpt_references_for_item(self, item, wpt_prefix, url_base):
        """Translate a manifest item's `references` (URL/relation pairs) into
        Reference objects whose paths are absolute filesystem paths."""
        refs = getattr(item, "references", None)
        if not refs:
            return None

        result = []
        for ref_url, relation in refs:
            test_path = self._wpt_url_to_test_path(ref_url, wpt_prefix, url_base)
            if test_path is None:
                continue
            # Strip any query/fragment from the reference path before resolving
            # to a filesystem path.
            file_part, _ = test_name_and_variant(test_path)
            abs_path = self.fs.join(
                self.layout_tests_base_dir, *file_part.split("/")
            )
            result.append(Reference(relation=relation, path=abs_path))
        return result

    def _baselines_for_test(self, basename, variant, non_test_files_by_search_path):
        """Find -expected.{txt,webarchive,png,wav} siblings for a test in the
        layered search paths. Returns
        (expected_text_path, expected_image_path, expected_audio_path) —
        the text slot prefers .txt over .webarchive when both exist."""

        expected_without_ext = TestResultWriter.expected_filename(
            basename + variant, self.fs, suffix=""
        )
        assert expected_without_ext[-1] == "."
        expected_without_ext = expected_without_ext[:-1]

        expected_text_path = None
        expected_webarchive_path = None
        expected_image_path = None
        expected_audio_path = None

        txt_name = expected_without_ext + ".txt"
        webarchive_name = expected_without_ext + ".webarchive"
        png_name = expected_without_ext + ".png"
        wav_name = expected_without_ext + ".wav"

        # This is inefficient. We search all non-test files in the directory for each variant of each test!
        for dirname, basename_set in reversed(non_test_files_by_search_path.items()):
            if expected_text_path is None and txt_name in basename_set:
                expected_text_path = self.fs.normpath(self.fs.join(dirname, txt_name))

            if expected_webarchive_path is None and webarchive_name in basename_set:
                expected_webarchive_path = self.fs.normpath(self.fs.join(dirname, webarchive_name))

            if expected_image_path is None and png_name in basename_set:
                expected_image_path = self.fs.normpath(self.fs.join(dirname, png_name))

            if expected_audio_path is None and wav_name in basename_set:
                expected_audio_path = self.fs.normpath(self.fs.join(dirname, wav_name))

        return (
            expected_text_path or expected_webarchive_path,
            expected_image_path,
            expected_audio_path,
        )

    def _sibling_references_for_test(self, basename, variant, non_test_files_by_search_path):
        """Find -expected.{html,htm,svg,xht,xhtml,xml} sibling reference files
        in the layered search paths. Returns a list of Reference objects
        from the most-specific directory containing any siblings, or None
        when no siblings exist."""

        reference_base = TestResultWriter.expected_filename(
            basename, self.fs, suffix=""
        )
        assert reference_base[-1] == "."
        reference_base = reference_base[:-1]

        match_reference_basenames = {
            reference_base + ext for ext in supported_reference_extensions
        }
        mismatch_reference_basenames = {
            "".join((reference_base, "-mismatch", ext))
            for ext in supported_reference_extensions
        }

        for dirname, basename_set in reversed(non_test_files_by_search_path.items()):
            matches = basename_set & match_reference_basenames
            mismatches = basename_set & mismatch_reference_basenames
            if matches or mismatches:
                # For historic reasons, we return matches first, and we sort them by
                # the filename of the match. This is significant when we currently
                # only run the first reference
                # (https://bugs.webkit.org/show_bug.cgi?id=270794).
                return [
                    Reference(relation="==", path=self.fs.join(dirname, m + variant)) for m in sorted(matches)
                ] + [Reference(relation="!=", path=self.fs.join(dirname, m + variant)) for m in sorted(mismatches)]

        return None

    def _expectations_for_test(self, basename, variant, non_test_files_by_search_path):
        """Find both baselines and sibling references for a test. Returns
        (expected_text_path, expected_image_path, expected_audio_path,
         reference_files) — see _baselines_for_test and
        _sibling_references_for_test for the per-half semantics."""

        text, image, audio = self._baselines_for_test(
            basename, variant, non_test_files_by_search_path
        )
        references = self._sibling_references_for_test(
            basename, variant, non_test_files_by_search_path
        )
        return (text, image, audio, references)

