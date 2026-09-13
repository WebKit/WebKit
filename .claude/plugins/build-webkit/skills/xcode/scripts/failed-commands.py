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

"""Report the failed part of an .xcresult bundle's log, as JSON on stdout.

Errors that do not produce semantic information in the result bundle are
only discoverable by examining command output. This script dumps all commands
and produces output info from failing tasks in the build, as JSON. Output is
truncated to avoid polluting context, but an agent can ask for detailed info
about a particular step by passing `--step STEPNAME` and rerunning.
"""

import argparse
import collections
import itertools
import json
import re
import subprocess
import sys

# Max number of failing steps to report the name of.
STEP_NAMES = 10

# When reporting error lines, ignore the include trace.
INCLUDE_STACK = re.compile(r'^In file included from |^In module ')

Limits = collections.namedtuple(
    'Limits',
    'errors_per_step steps_per_level output_lines output_lines_beside_issues line_length')

# Output limits for the default mode (reporting on all failed steps).
READABLE = Limits(errors_per_step=3, steps_per_level=5, output_lines=40,
                  output_lines_beside_issues=10, line_length=400)

# Output limits in --step filtering mode.
DETAILED = Limits(errors_per_step=15, steps_per_level=5, output_lines=150,
                  output_lines_beside_issues=150, line_length=2000)

# Limit to the number of matches found in --step mode.
MATCHED_STEPS = 3


def build_log(bundle):
    """Return a bundle's build log, or explain why it could not be read and stop."""
    result = subprocess.run(
        ['xcrun', 'xcresulttool', 'get', 'log', '--path', bundle, '--type', 'build'],
        capture_output=True, text=True)
    if result.returncode:
        error = result.stderr.strip()
        if 'No build log' in error:
            sys.exit('{} holds no build log, so the build never started. '
                     'Read the log instead.'.format(bundle))
        # Only the first line: the rest of what xcresulttool prints is its own usage.
        sys.exit('could not read {}: {}'.format(
            bundle, (error.splitlines() or ['xcresulttool failed'])[0]))
    return json.loads(result.stdout)


def clip(line, length):
    return line if length is None or len(line) <= length else line[:length] + ' […]'


def cap(items, limit):
    """Return as many items as the limit allows, and how many are left over."""
    if limit is None:
        return items, 0
    return items[:limit], max(0, len(items) - limit)


def diagnosis(lines):
    """Return the lines that say something, with the include stack framing them dropped."""
    return [line for line in lines if line.strip() and not INCLUDE_STACK.match(line.lstrip())]


def one_line(lines):
    """Return lines as a single line, with no run of whitespace longer than a space."""
    return ' '.join(' '.join(lines).split())


def collapse(lines):
    """Return lines with each run of identical ones reported once: a command asked to build for
    several architectures at once prints its diagnosis once for each of them."""
    collapsed = []
    for line, run in itertools.groupby(lines):
        repeats = sum(1 for _ in run) - 1
        collapsed.append(line)
        if repeats:
            collapsed.append('[previous line repeated {} more time{}]'.format(
                repeats, '' if repeats == 1 else 's'))
    return collapsed


def errors_of(section, limits):
    """Return the errors a step reported, counting a message repeated within it rather than
    listing it again.
    """
    errors = []
    for message in section.get('messages') or []:
        if message.get('type') != 'error':
            continue
        # Truncate very long diagnostic messages.
        error = {'title': clip(one_line(diagnosis((message.get('title') or '').splitlines())),
                               limits.line_length)}
        # Only some messages are categorized, and the ones that are not say so by saying nothing.
        if message.get('category'):
            error['category'] = message['category']
        repeat = next((seen for seen in errors if seen['title'] == error['title']
                       and seen.get('category') == error.get('category')), None)
        if repeat:
            repeat['count'] = repeat.get('count', 1) + 1
        else:
            errors.append(error)
    return errors


def failures_in(sections, limits):
    """Recurse into a step and return all (sub)steps which denote failure.
    """
    found = []
    for section in sections:
        if section.get('result') == 'failed':
            step = failure(section, limits)
            if step:
                found.append(step)
        else:
            found.extend(failures_in(section.get('subsections') or [], limits))
    return found


def failure(section, limits):
    """Return what a failed step says about why it failed, along with its failed substeps.

    A failed step might have no failure information attached; the cause is
    usually in one of its substeps.
    """
    invocation = section.get('commandInvocationDetails') or {}
    output = collapse(diagnosis((invocation.get('emittedOutput') or '').splitlines()))
    issues, unreported_issues = cap(errors_of(section, limits), limits.errors_per_step)
    steps, unreported_steps = cap(failures_in(section.get('subsections') or [], limits),
                                  limits.steps_per_level)

    if not output and not issues and not steps:
        return None

    step = {}
    # Title may be long for steps that compile multiple files (e.g. Swift batch
    # compilation).
    if section.get('title'):
        step['step'] = clip(section['title'], limits.line_length)
    # commandDetails names the target and project the command ran for, where the title above names
    # only the step. It can carry a whole invocation, so keep its first line.
    command = (invocation.get('commandDetails') or '').splitlines()
    if command:
        step['command'] = clip(command[0], limits.line_length)
    if invocation.get('exitCode') is not None:
        step['exit'] = invocation['exitCode']
    if issues:
        step['issues'] = issues
        if unreported_issues:
            step['unreported_issues'] = unreported_issues
    if output:
        printed, unprinted = cap(
            output, limits.output_lines_beside_issues if issues else limits.output_lines)
        step['output'] = [clip(line, limits.line_length) for line in printed]
        if unprinted:
            step['unprinted_output_lines'] = unprinted
    if steps:
        step['steps'] = steps
        if unreported_steps:
            step['unreported_steps'] = unreported_steps
    return step


def steps_mentioning(steps, text):
    """Return the failed steps whose name or command mentions text, wherever in
    the tree they are.
    """
    wanted = text.lower()
    found = []
    for step in steps:
        if wanted in '{} {}'.format(step.get('step', ''), step.get('command', '')).lower():
            found.append(step)
        else:
            found.extend(steps_mentioning(step.get('steps') or [], text))
    return found


def names_in(steps):
    """Return the name of every failed step. Used to suggest alternative --step
    matches.
    """
    names = []
    for step in steps:
        if step.get('step'):
            names.append(clip(step['step'], READABLE.line_length))
        names.extend(names_in(step.get('steps') or []))
    return names


def report(bundle, wanted_step):
    log = build_log(bundle)

    if wanted_step is None:
        failures, unreported = cap(failures_in([log], READABLE), READABLE.steps_per_level)
        # What the steps say, rather than what the root of the log claims. No failures under a
        # build that did fail means it died outside of any step of itself, and the log build.sh
        # wrote is the only account of that.
        summary = {'result': 'failed' if failures else log.get('result'), 'failures': failures}
        if unreported:
            summary['unreported_failures'] = unreported
        return summary

    everything = failures_in([log], DETAILED)
    failures, unreported = cap(steps_mentioning(everything, wanted_step), MATCHED_STEPS)
    summary = {'result': 'failed' if everything else log.get('result'), 'failures': failures}
    if unreported:
        summary['unreported_failures'] = unreported
    if not failures:
        names = names_in(everything)
        summary['note'] = 'No failed step mentions {!r}. The failed steps are: {}'.format(
            wanted_step, '; '.join(names[:STEP_NAMES]) or 'none')
    return summary


def parse_arguments():
    parser = argparse.ArgumentParser(
        description='Report the failed part of a build log, as JSON on stdout.')
    parser.add_argument('bundle', metavar='XCRESULT', help='the .xcresult bundle to read')
    parser.add_argument('--step', metavar='TEXT',
                        help='report only the failed steps whose name or command mentions TEXT, '
                             'and report much more of their errors and output than a summary does')
    return parser.parse_args()


if __name__ == '__main__':
    arguments = parse_arguments()
    print(json.dumps(report(arguments.bundle, arguments.step), indent=2, ensure_ascii=False))
