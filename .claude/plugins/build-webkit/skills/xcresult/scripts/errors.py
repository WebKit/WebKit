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

import argparse
import collections
import json
import subprocess
import sys
import urllib.parse
import urllib.request

# Truncate diagnostic strings longer than this length.
MESSAGE_LENGTH = 250

# If the same error is reported multiple times at different locations, it will
# only be reported this many times.
PLACES_PER_MESSAGE = 10

# Paginate the output to avoid flooding the context.
MESSAGES_PER_PAGE = 20

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

    # Most sourceURLs name a place in a file, with fragments indicating line
    # info and timestamp:
    #
    #   file:///Users/emw/WebKit/Source/WTF/wtf/text/WTFString.cpp#EndingColumnNumber=14
    #       &EndingLineNumber=219&StartingColumnNumber=14&StartingLineNumber=219
    #       &Timestamp=811471337.012409
    #
    # The rest name a target's build settings rather than a line of code, and carry no line at all:
    #
    #   file:///Users/emw/WebKit/Source/WTF/WTF.xcodeproj#Timestamp=811471337.012409
    #       &XcodeLocation=%7B%22Selection%22:%7B%22Editor%22:%22Xcode3BuildSettingsEditor%22
    #       ,%22Targets%22:%5B%22WTF%22%5D,%22Xcode3BuildSettingsEditorLocations%22
    #       :%5B%7B%22Selected%20Build%20Properties%22:%5B%22ENABLE_USER_SCRIPT_SANDBOXING%22%5D
    #       %7D%5D%7D%7D
    location = urllib.parse.urlsplit(url)
    path = urllib.request.url2pathname(location.path)

    lines = urllib.parse.parse_qs(location.fragment).get('StartingLineNumber')
    if not lines or not lines[0].isdigit():
        return path, None
    # A DocumentLocation fragment counts lines from zero, where compilers and editors count from one.
    return path, int(lines[0]) + 1


def place(error):
    """Return where an error is, written as a compiler would print it."""
    if error['line'] is None:
        return error['path']
    return '{}:{}'.format(error['path'], error['line'])


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
            'path': path,
            'line': line,
        })
    return records


def report(build, offset):
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
    elif errors and not any(error['path'] for error in errors):
        summary['note'] = 'No error names a source file, so the cause is only in the build log: ' \
                          'run failed-commands.py.'

    if not errors:
        return summary

    summary['by_file'] = dict(collections.Counter(
        error['path'] or NO_SOURCE for error in errors).most_common())

    repeats = collections.OrderedDict()
    for error in errors:
        repeats.setdefault(error['message'], []).append(error)

    messages = []
    for message, group in sorted(repeats.items(), key=lambda repeat: -len(repeat[1])):
        entry = {'category': group[0]['category'], 'message': message, 'count': len(group)}
        places = [place(error) for error in group if error['path']]
        if places:
            entry['places'] = places[:PLACES_PER_MESSAGE]
        messages.append(entry)

    # Messages are ordered by how often they repeat, ties in the order the bundle records them in,
    # so every run pages through them in the same order.
    summary['messages'] = messages[offset:offset + MESSAGES_PER_PAGE]
    unreported = len(messages) - offset - len(summary['messages'])
    if unreported > 0:
        summary['unreported_messages'] = unreported
        summary['next_offset'] = offset + len(summary['messages'])

    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(
        description='Report the errors an .xcresult bundle recorded, as JSON on stdout.')
    parser.add_argument('bundle', metavar='XCRESULT', help='the .xcresult bundle to read')
    parser.add_argument('--offset', metavar='N', type=int, default=0,
                        help='start the messages at the Nth, to read the ones a previous run '
                             'reported as unreported_messages')
    arguments = parser.parse_args()
    if arguments.offset < 0:
        parser.error('--offset cannot be negative')
    return arguments


if __name__ == '__main__':
    arguments = parse_arguments()
    print(json.dumps(report(build_results(arguments.bundle), arguments.offset),
                     indent=2, ensure_ascii=False))
