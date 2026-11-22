# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Run a local AI coding agent as a code reviewer and fold its findings into a
review diff.

The diff annotation, anchoring and finding-folding here work on the materialized
review diff that pull_request.diff(comments=True) yields, which is the one
representation every remote converges on, so this module is independent of both
the review program and the remote hosting the pull request. The only backend
currently wired up is the Claude Code CLI; everything specific to it is confined
to _run_claude_cli() so the rest of the module stays backend-neutral."""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

from collections import defaultdict

from webkitcorepy import run as run_command, string_utils

# The marker prefixed to every folded finding in the review file. It (and any
# '[severity]' tag that follows) is stripped from comments the reviewer keeps so
# they post as clean prose.
COMMENT_PREFIX = 'agent: '

# The line prefix used to show the agent review comments already on the pull
# request, so it does not re-raise points that have been made.
PRIOR_COMMENT_PREFIX = '# prior comment: '

# The system prompt: what the agent should do, independent of which pull request
# it is looking at. The pull request itself is passed as the user prompt.
REVIEW_INSTRUCTIONS = f"""\
You are performing a rigorous, comprehensive code review of a pull request, run \
locally inside the repository the PR modifies. The working tree is the \
PRE-CHANGE state of the codebase, so you can read any file, grep, and follow \
references to understand context the diff alone does not show.

The checkout is not guaranteed to be at the exact commit the diff was taken \
against, so treat the diff as authoritative for what is changing and verify \
every finding against the file as you actually read it. Say so rather than \
reporting a finding if the two disagree.

Cover, with equal seriousness, everything that a thorough correctness review AND \
a thorough simplification/cleanup review would surface:
  - Correctness & logic bugs, off-by-one, null handling, error paths, \
concurrency, resource leaks, security issues.
  - Reuse, simplification, and efficiency: duplicated logic, dead code, needless \
complexity, redundant work, simpler equivalent constructs.
  - Conventions & clarity: adherence to nearby code style, naming, confusing \
structure, missing or misleading comments.
  - Test gaps and edge cases the change introduces or fails to handle.

Only report issues you have verified by reading the surrounding code. Do not \
nitpick things a linter/compiler would catch (imports, formatting, type errors). \
Skip pre-existing issues on lines the PR did not touch. Prefer fewer, \
high-signal findings over volume, but be thorough across the categories above. \
Each finding must be anchored to a specific changed line.

The diff tags every line with its file and absolute line number: 'N<n>' is a \
line number in the new (post-change) file, 'O<n>' is a line number in the old \
(pre-change) file. Anchor each comment to one of those tagged lines: use side \
"new" with the N<n> number for added and context lines, side "old" with the \
O<n> number for removed lines.

Lines prefixed '{PRIOR_COMMENT_PREFIX.strip()}' are review comments already on \
the pull request. They are context only: do not repeat a point that has already \
been made, and do not anchor a finding to them.\
"""

# The findings contract, enforced by the backend rather than by asking the model
# to format its reply correctly.
FINDINGS_SCHEMA = {
    'type': 'object',
    'properties': {
        'comments': {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'file': {'type': 'string'},
                    'line': {'type': 'integer'},
                    'side': {'type': 'string', 'enum': ['new', 'old']},
                    'severity': {'type': 'string', 'enum': ['bug', 'cleanup', 'nit', 'question']},
                    'text': {'type': 'string'},
                },
                'required': ['file', 'line', 'side', 'severity', 'text'],
                'additionalProperties': False,
            },
        },
    },
    'required': ['comments'],
    'additionalProperties': False,
}


def strip_line(line):
    """Strip the 'agent: ' marker (and a leading '[severity] ' tag) from a single
    line, so that kept agent findings post as clean prose."""
    if line.startswith(COMMENT_PREFIX):
        line = line[len(COMMENT_PREFIX):]
        line = re.sub(r'^\[[^\]]+\]\s*', '', line)
    return line


def parse_json(text):
    """Parse text as JSON, returning (value, error): value is the parsed value, or
    None with a short human-readable reason in error."""
    if not text or not text.strip():
        return None, 'empty text'
    try:
        return json.loads(text), None
    except Exception as e:
        return None, str(e)


def _header_path(old_line, new_line):
    """Return the path a '--- <old>' / '+++ <new>' file-header pair refers to, or
    None if the pair does not look like one. The new side is preferred so that a
    rename anchors to its new path, but a deleted file's new side is /dev/null,
    in which case the path comes from the old side."""
    for line, marker in ((new_line, '+++ b/'), (old_line, '--- a/')):
        if line.startswith(marker):
            return line[len(marker):]
    return None


def diff_maps(diff_lines):
    """Walk the materialized review diff (which already contains any >>>>/<<<<
    PR-comment blocks) and return:
      annotated: a line-numbered rendering of the diff for the model, each
                 content line tagged 'N<dest>' or 'O<src>'.
      anchor: {(side, file, abs_line): index} -> the diff_lines index whose line
              a comment should be inserted after.
      file_header: {file: index} of each file's new-side header line, for
                   file-level fallbacks when an anchor line is not visible in
                   the diff."""
    annotated = []
    anchor = {}
    file_header = {}
    file = None
    in_comment = False
    new_no = old_no = None
    for idx, line in enumerate(diff_lines):
        if line == '>>>>':
            in_comment = True
            continue
        if line == '<<<<':
            in_comment = False
            continue
        if in_comment:
            # Existing PR comments are shown to the model as context, but they are
            # not part of the file, so they neither advance the line counters nor
            # become anchors.
            annotated.append(f'{PRIOR_COMMENT_PREFIX}{line}')
            continue
        # A file-header pair is a '--- <old>' line immediately followed by a
        # '+++ <new>' line. Keying header detection on that adjacency avoids
        # mistaking an added content line whose text happens to start with
        # '++ b/' (rendered '+++ b/...') for a new file header, and likewise a
        # removed line starting with '-- '.
        if line.startswith('+++ ') and idx and diff_lines[idx - 1].startswith('--- '):
            path = _header_path(diff_lines[idx - 1], line)
            if path:
                file = path
                file_header[file] = idx
                new_no = old_no = None
                annotated.append('')
                annotated.append(f'### File: {file}')
                continue
        if line.startswith('diff ') or line.startswith('index '):
            continue
        # The old side of a header pair carries no information the new side does
        # not, so it is dropped; the same adjacency and path checks apply, or a
        # removed line rendering as '--- ...' would be dropped as a header.
        if line.startswith('--- ') and idx + 1 < len(diff_lines) and diff_lines[idx + 1].startswith('+++ '):
            if _header_path(line, diff_lines[idx + 1]):
                continue
        if line.startswith('@@'):
            match = re.match(r'@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@', line)
            if match:
                old_no = int(match.group(1))
                new_no = int(match.group(2))
            annotated.append(line)
            continue
        if file is None or new_no is None:
            continue
        leader = line[:1]
        body = line[1:] if leader in (' ', '+', '-') else line
        if leader == '+':
            anchor[('destination', file, new_no)] = idx
            tag = f'N{new_no}'
            annotated.append(f'{tag:>8} + {body}')
            new_no += 1
        elif leader == '-':
            anchor[('source', file, old_no)] = idx
            tag = f'O{old_no}'
            annotated.append(f'{tag:>8} - {body}')
            old_no += 1
        else:
            anchor[('destination', file, new_no)] = idx
            anchor[('source', file, old_no)] = idx
            tag = f'N{new_no}'
            annotated.append(f'{tag:>8}   {body}')
            new_no += 1
            old_no += 1
    return '\n'.join(annotated), anchor, file_header


def annotate_diff(diff_lines, findings):
    """Fold a list of finding dicts into a copy of diff_lines as plain 'agent: '
    lines anchored after the relevant diff line. Returns (rebuilt, placed,
    skipped): rebuilt is the annotated diff (None if nothing could be placed),
    placed is the number of findings folded in, and skipped counts findings that
    fell back to a file-level anchor because their line was not in the diff."""
    _, anchor, file_header = diff_maps(diff_lines)

    insertions = defaultdict(list)
    placed = 0
    skipped = 0
    for c in findings:
        if not isinstance(c, dict):
            continue
        path = c.get('file')
        line = c.get('line')
        side = 'source' if c.get('side') == 'old' else 'destination'
        text = (c.get('text') or '').strip()
        if not path or not text:
            continue
        try:
            line = int(line)
        except (TypeError, ValueError):
            line = None
        # Source and destination line numbers are counted independently, so a
        # line number that misses on the side the finding names cannot be looked
        # up on the other side: a hit there would be a coincidence. Fall back to
        # the file instead, which is at least honest about being approximate.
        idx = anchor.get((side, path, line)) if line is not None else None
        if idx is None:
            idx = file_header.get(path)
            if idx is None:
                continue
            skipped += 1
        severity = c.get('severity')
        body = f'[{severity}] {text}' if severity else text
        body_lines = body.splitlines() or ['']
        insertions[idx].extend([COMMENT_PREFIX + body_lines[0]] + body_lines[1:])
        placed += 1

    if not insertions:
        return None, 0, skipped

    # Rebuild the diff, dropping each insertion AFTER the target line and after
    # any existing >>>>/<<<< comment block already attached to it.
    rebuilt = []
    i = 0
    n = len(diff_lines)
    while i < n:
        rebuilt.append(diff_lines[i])
        j = i + 1
        if j < n and diff_lines[j] == '>>>>':
            while j < n and diff_lines[j] != '<<<<':
                rebuilt.append(diff_lines[j])
                j += 1
            if j < n:  # the closing '<<<<'
                rebuilt.append(diff_lines[j])
                j += 1
        rebuilt.extend(insertions.get(i, []))
        i = j

    return rebuilt, placed, skipped


def _debug_dump(content):
    """Write content to a per-process temp file for post-mortem inspection and
    return the path (returned even if the write failed, so callers can still
    name it in a warning)."""
    path = os.path.join(tempfile.gettempdir(), f'git-webkit-agent-review-{os.getpid()}.txt')
    try:
        with open(path, 'w') as f:
            f.write(content)
    except Exception:
        pass
    return path


# Tools the agent is allowed to use: Read/Grep/Glob plus a set of non-mutating
# git subcommands, expressed in the Claude Code CLI --allowedTools syntax.
# Anything that can mutate the repository (commit, push, update-ref, branch,
# reset, ...) is deliberately excluded so the review cannot alter the checkout it
# is inspecting, and combined with '--permission-mode dontAsk' anything not
# listed here is denied rather than prompted for.
_CLAUDE_ALLOWED_TOOLS = [
    'Read', 'Grep', 'Glob',
    'Bash(git log:*)',
    'Bash(git show:*)',
    'Bash(git diff:*)',
    'Bash(git blame:*)',
    'Bash(git grep:*)',
    'Bash(git cat-file:*)',
    'Bash(git ls-files:*)',
    'Bash(git ls-tree:*)',
    'Bash(git rev-parse:*)',
]

# How many lines of the CLI's stderr to relay when it exits non-zero.
_STDERR_LINES = 20


def _result_message(envelope):
    """Pick the terminal 'result' message out of the CLI's --output-format json
    output. That is a single result object, unless the reviewer has verbose
    output enabled, in which case the CLI emits the whole message array
    instead."""
    if isinstance(envelope, dict):
        return envelope
    if isinstance(envelope, list):
        for message in reversed(envelope):
            if isinstance(message, dict) and message.get('type') == 'result':
                return message
    return None


def _run_claude_cli(system_prompt, user_prompt, repo_root):
    """Run the Claude Code CLI headless and read-only against repo_root, returning
    the review findings it produced (as a JSON value matching FINDINGS_SCHEMA) or
    None on any failure. Everything specific to the Claude Code CLI, its
    executable, argv and output envelope, is confined here."""
    claude_bin = shutil.which('claude')
    if not claude_bin:
        sys.stderr.write("Warning: 'claude' CLI not found on PATH; skipping --agent review.\n")
        return None

    # --allowedTools takes a variable number of values, so it goes last.
    cmd = [claude_bin, '-p',
           '--output-format', 'json',
           '--json-schema', json.dumps(FINDINGS_SCHEMA),
           '--append-system-prompt', system_prompt,
           '--permission-mode', 'dontAsk',
           '--allowedTools'] + _CLAUDE_ALLOWED_TOOLS

    print('Running agent review (read-only, this can take a few minutes)...')
    try:
        proc = run_command(cmd, input=user_prompt, capture_output=True,
                           encoding='utf-8', cwd=repo_root, timeout=900)
    except subprocess.TimeoutExpired:
        sys.stderr.write('Warning: Agent review timed out; continuing without annotations.\n')
        return None
    except Exception as e:
        sys.stderr.write(f'Warning: Agent review failed to run ({e}); continuing.\n')
        return None

    if proc.returncode != 0:
        sys.stderr.write(f'Warning: Agent review exited with status {proc.returncode}.\n')
        err = proc.stderr.strip().splitlines()
        if len(err) > _STDERR_LINES:
            sys.stderr.write(f'  (last {_STDERR_LINES} of {len(err)} stderr lines)\n')
            err = err[-_STDERR_LINES:]
        for line in err:
            sys.stderr.write(f'  {line}\n')
        return None

    raw = proc.stdout or ''
    envelope, error = parse_json(raw)
    result = _result_message(envelope)
    if result is None:
        debug_path = _debug_dump(raw)
        reason = error or 'no terminal result message in the output'
        sys.stderr.write(f'Warning: Could not parse the agent CLI output envelope ({reason}).\n')
        sys.stderr.write(f'  Captured {len(raw)} bytes of stdout; saved to {debug_path}\n')
        if raw.strip():
            preview = raw.strip()[:300].replace('\n', ' ')
            sys.stderr.write(f'  Output begins: {preview}\n')
        return None

    subtype = result.get('subtype')
    if result.get('is_error') or (subtype and subtype != 'success'):
        is_error = result.get('is_error')
        sys.stderr.write(f'Warning: Agent review reported a problem (subtype={subtype}, is_error={is_error}).\n')

    # The schema is validated by the CLI, which hands back the parsed findings.
    findings = result.get('structured_output')
    if findings is not None:
        return findings

    # No structured output: the CLI rejected the schema and fell back to plain
    # text, so parse the reply as JSON ourselves.
    text = result.get('result')
    if isinstance(text, str) and text.strip():
        findings, error = parse_json(text)
        if findings is not None:
            return findings
        debug_path = _debug_dump(text)
        sys.stderr.write(f"Warning: Could not parse the agent's review findings ({error}).\n")
        sys.stderr.write(f"  Agent's reply ({len(text)} chars) saved to {debug_path}\n")
        return None

    debug_path = _debug_dump(json.dumps(result, indent=2, sort_keys=True))
    sys.stderr.write(f'Warning: Agent returned no findings to parse (subtype={subtype}).\n')
    sys.stderr.write(f'  Full envelope saved to {debug_path}\n')
    return None


def _user_prompt(pull_request, annotated_diff):
    """The pull request under review, as the user half of the prompt. The content
    is interpolated once and never re-scanned, so nothing a PR author writes can
    be mistaken for part of the review instructions."""
    return '\n'.join([
        '=== PULL REQUEST ===',
        f'Title: {pull_request.title or ""}',
        '',
        'Description:',
        pull_request.body or '(none)',
        '',
        '=== ANNOTATED DIFF ===',
        annotated_diff,
    ])


def run(pull_request, diff_lines, repo_root=None):
    """Run a headless, read-only AI agent code review of the PR and return a copy
    of diff_lines with the agent's findings folded in as plain 'agent:' lines
    anchored to the relevant diff lines. Returns None on any failure, so the
    caller falls back to the normal un-annotated review file. repo_root is the
    directory the agent runs in, defaulting to the current working directory."""
    annotated, _, _ = diff_maps(diff_lines)

    findings = _run_claude_cli(
        REVIEW_INSTRUCTIONS, _user_prompt(pull_request, annotated),
        repo_root or os.getcwd(),
    )
    if findings is None:
        return None

    found = findings.get('comments') if isinstance(findings, dict) else findings
    if not isinstance(found, list):
        debug_path = _debug_dump(json.dumps(findings, indent=2, sort_keys=True))
        sys.stderr.write(
            f"Warning: The agent's findings had no 'comments' list "
            f'(top-level type was {type(findings).__name__}).\n')
        sys.stderr.write(f'  Findings saved to {debug_path}\n')
        return None

    rebuilt, placed, skipped = annotate_diff(diff_lines, found)
    if rebuilt is None:
        print('Agent review: no comments to add.')
        return None

    counted = string_utils.pluralize(placed, 'comment')
    print(f'Agent review: {counted} added ({skipped} not anchored to a visible diff line).')
    return rebuilt
