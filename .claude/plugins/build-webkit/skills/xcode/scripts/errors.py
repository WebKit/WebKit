#!/usr/bin/env python3
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

"""Report semantic errors an .xcresult bundle recorded, if any, as JSON on stdout.
First step in analysis, which does not require expanding the entire build log.

Some errors do not carry semantic data and are only present in the textual log.
The sibling `failed-commands.py` expands those.

Care is taken to avoid injecting too much output into the context, by
grouping repeat failures and truncating very long messages.
"""

import collections
import json
import os
import subprocess
import sys
import urllib.parse

# Truncate diagnostic strings longer than this length.
MESSAGE_LENGTH = 250

# If the same error is reported multiple times at different locations, it will
# only be reported this many times.
PLACES_PER_MESSAGE = 10

NO_SOURCE = '(no source location)'


def build_results(bundle):
    """Return a bundle's build-results, or explain why it could not be read and stop."""
    result = subprocess.run(
        ['xcrun', 'xcresulttool', 'get', 'build-results', '--path', bundle],
        capture_output=True, text=True)
    if result.returncode:
        # Only the first line: the rest of what xcresulttool prints is its own usage.
        reason = (result.stderr.strip().splitlines() or ['xcresulttool failed'])[0]
        sys.exit('could not read {}: {}'.format(bundle, reason))
    return json.loads(result.stdout)


def source(issue):
    """Return the (path, line) an issue is at, either of which can be None."""
    url = issue.get('sourceURL')
    if not url:
        return None, None

    path, _, fragment = url.partition('#')
    path = urllib.parse.unquote(path)
    if path.startswith('file://'):
        path = path[len('file://'):]

    lines = urllib.parse.parse_qs(fragment).get('StartingLineNumber')
    if not lines or not lines[0].isdigit():
        return path, None
    # A DocumentLocation fragment counts lines from zero, where compilers and editors count from one.
    return path, int(lines[0]) + 1


def clip(message):
    collapsed = ' '.join(message.split())
    if len(collapsed) <= MESSAGE_LENGTH:
        return collapsed
    return collapsed[:MESSAGE_LENGTH] + ' […]'


def errors_of(build):
    """Return one record per error: where it is, and what it says."""
    records = []
    for error in build.get('errors') or []:
        path, line = source(error)
        records.append({
            'category': error.get('issueType'),
            'message': clip(error.get('message') or ''),
            'file': os.path.basename(path) if path else NO_SOURCE,
            'place': None if not path else path if line is None else '{}:{}'.format(path, line),
        })
    return records


def report(build):
    errors = errors_of(build)
    summary = {
        'status': build.get('status'),
        'action': build.get('actionTitle'),
        'errors': build.get('errorCount', 0),
        'warnings': build.get('warningCount', 0),
    }

    if build.get('status') == 'notRequested':
        # A run that died before xcodebuild started building leaves a bundle like this one: no
        # action requested, an actionTitle of '(Transient Testing)', and at most a generic
        # "xcodebuild encountered an error" issue. Nothing about the cause is recorded in it.
        summary['note'] = 'This bundle is empty and the build never started; ' \
                          'the reason is in the log.'
    elif errors and not any(error['place'] for error in errors):
        summary['note'] = 'No error names a source file, so the cause is only in the build log: ' \
                          'run failed-commands.py.'

    if not errors:
        return summary

    summary['by_file'] = dict(collections.Counter(
        error['file'] for error in errors).most_common())

    repeats = collections.OrderedDict()
    for error in errors:
        repeats.setdefault(error['message'], []).append(error)

    summary['messages'] = []
    for message, group in sorted(repeats.items(), key=lambda repeat: -len(repeat[1])):
        entry = {'category': group[0]['category'], 'message': message, 'count': len(group)}
        places = [error['place'] for error in group if error['place']]
        if places:
            entry['places'] = places[:PLACES_PER_MESSAGE]
        summary['messages'].append(entry)

    return summary


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: {} <path to .xcresult>'.format(os.path.basename(sys.argv[0])))
    print(json.dumps(report(build_results(sys.argv[1])), indent=2, ensure_ascii=False))
