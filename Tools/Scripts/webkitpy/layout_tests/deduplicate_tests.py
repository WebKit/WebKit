#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""deduplicate_tests -- lists duplicated between platforms.

If platform/mac-leopard is missing an expected test output, we fall back on
platform/mac.  This means it's possible to grow redundant test outputs,
where we have the same expected data in both a platform directory and another
platform it falls back on.
"""

import collections
import fnmatch
import os
import subprocess
import sys
import re

from webkitpy.common.checkout import scm
from webkitpy.common.system import executive
from webkitpy.common.system import logutils
from webkitpy.common.system import ospath
from webkitpy.layout_tests.port import factory as port_factory

_log = logutils.get_logger(__file__)

_BASE_PLATFORM = 'base'


def port_fallbacks():
    """Get the port fallback information.
    Returns:
        A dictionary mapping platform name to a list of other platforms to fall
        back on.  All platforms fall back on 'base'.
    """
    fallbacks = {_BASE_PLATFORM: []}
    for port_name in port_factory.all_port_names():
        try:
            platforms = port_factory.get(port_name).baseline_search_path()
        except NotImplementedError:
            _log.error("'%s' lacks baseline_search_path(), please fix."
                       % port_name)
            fallbacks[port_name] = [_BASE_PLATFORM]
            continue
        fallbacks[port_name] = [os.path.basename(p) for p in platforms][1:]
        fallbacks[port_name].append(_BASE_PLATFORM)

    return fallbacks


def parse_git_output(git_output, glob_pattern):
    """Parses the output of git ls-tree and filters based on glob_pattern.
    Args:
        git_output: result of git ls-tree -r HEAD LayoutTests.
        glob_pattern: a pattern to filter the files.
    Returns:
        A dictionary mapping (test name, hash of content) => [paths]
    """
    hashes = collections.defaultdict(set)
    for line in git_output.split('\n'):
        if not line:
            break
        attrs, path = line.strip().split('\t')
        if not fnmatch.fnmatch(path, glob_pattern):
            continue
        path = path[len('LayoutTests/'):]
        match = re.match(r'^(platform/.*?/)?(.*)', path)
        test = match.group(2)
        _, _, hash = attrs.split(' ')
        hashes[(test, hash)].add(path)
    return hashes


def cluster_file_hashes(glob_pattern):
    """Get the hashes of all the test expectations in the tree.
    We cheat and use git's hashes.
    Args:
        glob_pattern: a pattern to filter the files.
    Returns:
        A dictionary mapping (test name, hash of content) => [paths]
    """

    # A map of file hash => set of all files with that hash.
    hashes = collections.defaultdict(set)

    # Fill in the map.
    cmd = ('git', 'ls-tree', '-r', 'HEAD', 'LayoutTests')
    try:
        git_output = executive.Executive().run_command(cmd,
            cwd=scm.find_checkout_root())
    except OSError, e:
        if e.errno == 2:  # No such file or directory.
            _log.error("Error: 'No such file' when running git.")
            _log.error("This script requires git.")
            sys.exit(1)
        raise e
    return parse_git_output(git_output, glob_pattern)


def dirname_to_platform(dirname):
    if dirname == 'chromium-linux':
        return 'chromium-linux-x86'
    elif dirname == 'chromium-win':
        return 'chromium-win-win7'
    elif dirname == 'chromium-mac':
        return 'chromium-mac-snowleopard'
    return dirname

def extract_platforms(paths):
    """Extracts the platforms from a list of paths matching ^platform/(.*?)/.
    Args:
        paths: a list of paths.
    Returns:
        A dictionary containing all platforms from paths.
    """
    platforms = {}
    for path in paths:
        match = re.match(r'^platform/(.*?)/', path)
        if match:
            platform = dirname_to_platform(match.group(1))
        else:
            platform = _BASE_PLATFORM
        platforms[platform] = path
    return platforms


def has_intermediate_results(test, fallbacks, matching_platform,
                             path_exists=os.path.exists):
    """Returns True if there is a test result that causes us to not delete
    this duplicate.

    For example, chromium-linux may be a duplicate of the checked in result,
    but chromium-win may have a different result checked in.  In this case,
    we need to keep the duplicate results.

    Args:
        test: The test name.
        fallbacks: A list of platforms we fall back on.
        matching_platform: The platform that we found the duplicate test
            result.  We can stop checking here.
        path_exists: Optional parameter that allows us to stub out
            os.path.exists for testing.
    """
    for dirname in fallbacks:
        platform = dirname_to_platform(dirname)
        if platform == matching_platform:
            return False
        test_path = os.path.join('LayoutTests', 'platform', dirname, test)
        if path_exists(test_path):
            return True
    return False


def get_relative_test_path(filename, relative_to,
                           checkout_root=scm.find_checkout_root()):
    """Constructs a relative path to |filename| from |relative_to|.
    Args:
        filename: The test file we're trying to get a relative path to.
        relative_to: The absolute path we're relative to.
    Returns:
        A relative path to filename or None if |filename| is not below
        |relative_to|.
    """
    layout_test_dir = os.path.join(checkout_root, 'LayoutTests')
    abs_path = os.path.join(layout_test_dir, filename)
    return ospath.relpath(abs_path, relative_to)


def find_dups(hashes, port_fallbacks, relative_to):
    """Yields info about redundant test expectations.
    Args:
        hashes: a list of hashes as returned by cluster_file_hashes.
        port_fallbacks: a list of fallback information as returned by
            get_port_fallbacks.
        relative_to: the directory that we want the results relative to
    Returns:
        a tuple containing (test, platform, fallback, platforms)
    """
    for (test, hash), cluster in hashes.items():
        if len(cluster) < 2:
            continue  # Common case: only one file with that hash.

        # Compute the list of platforms we have this particular hash for.
        platforms = extract_platforms(cluster)
        if len(platforms) == 1:
            continue

        # See if any of the platforms are redundant with each other.
        for platform in platforms.keys():
            if platform not in port_factory.all_port_names():
                continue
            for dirname in port_fallbacks[platform]:
                fallback = dirname_to_platform(dirname)
                if fallback not in platforms.keys():
                    continue
                # We have to verify that there isn't an intermediate result
                # that causes this duplicate hash to exist.
                if has_intermediate_results(test, port_fallbacks[platform],
                                            fallback):
                    continue
                # We print the relative path so it's easy to pipe the results
                # to xargs rm.
                path = get_relative_test_path(platforms[platform], relative_to)
                if not path:
                    continue
                yield {
                    'test': test,
                    'platform': platform,
                    'fallback': dirname,
                    'path': path,
                }


def deduplicate(glob_pattern):
    """Traverses LayoutTests and returns information about duplicated files.
    Args:
        glob pattern to filter the files in LayoutTests.
    Returns:
        a dictionary containing test, path, platform and fallback.
    """
    fallbacks = port_fallbacks()
    hashes = cluster_file_hashes(glob_pattern)
    return list(find_dups(hashes, fallbacks, os.getcwd()))
