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
import webkitpy.common.system.executive as executive
import webkitpy.common.system.logutils as logutils
import webkitpy.layout_tests.port.factory as port_factory

_log = logutils.get_logger(__file__)

_BASE_PLATFORM = 'base'


def port_fallbacks():
    """Get the port fallback information.
    Returns:
        A dictionary mapping platform name to a list of other platforms to fall
        back on.  All platforms fall back on 'base'.
    """
    fallbacks = {_BASE_PLATFORM: []}
    for port_name in os.listdir(os.path.join('LayoutTests', 'platform')):
        try:
            platforms = port_factory.get(port_name).baseline_search_path()
        except NotImplementedError:
            _log.error("'%s' lacks baseline_search_path(), please fix." % port_name)
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
        git_output = executive.Executive().run_command(cmd)
    except OSError, e:
        if e.errno == 2:  # No such file or directory.
            _log.error("Error: 'No such file' when running git.")
            _log.error("This script requires git.")
            sys.exit(1)
        raise e
    return parse_git_output(git_output, glob_pattern)


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
            platform = match.group(1)
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
    for platform in fallbacks:
        if platform == matching_platform:
            return False
        test_path = os.path.join('LayoutTests', 'platform', platform, test)
        if path_exists(test_path):
            return True
    return False


def find_dups(hashes, port_fallbacks):
    """Yields info about redundant test expectations.
    Args:
        hashes: a list of hashes as returned by cluster_file_hashes.
        port_fallbacks: a list of fallback information as returned by get_port_fallbacks.
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
            for fallback in port_fallbacks[platform]:
                if fallback in platforms.keys():
                    # We have to verify that there isn't an intermediate result
                    # that causes this duplicate hash to exist.
                    if not has_intermediate_results(test,
                            port_fallbacks[platform], fallback):
                        path = os.path.join('LayoutTests', platforms[platform])
                        yield test, platform, fallback, path


def deduplicate(glob_pattern):
    """Traverses LayoutTests and returns information about duplicated files.
    Args:
        glob pattern to filter the files in LayoutTests.
    Returns:
        a dictionary containing test, path, platform and fallback.
    """
    fallbacks = port_fallbacks()
    hashes = cluster_file_hashes(glob_pattern)
    return [{'test': test, 'path': path, 'platform': platform, 'fallback': fallback}
             for test, platform, fallback, path in find_dups(hashes, fallbacks)]
