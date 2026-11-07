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

from __future__ import annotations

import os
import re
import sys
import tempfile
import threading
from typing import IO, Any

if sys.version_info >= (3, 2):
    from html import escape
else:
    from cgi import escape

if sys.version_info >= (3, 0):
    from http.server import HTTPServer, BaseHTTPRequestHandler
else:
    from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler

from .diff import DiffBase

from webkitcorepy import Terminal


class HTMLDiff(DiffBase):
    HEAD = '''<html>
<head>
<meta charset='utf-8'>
<style>
:root {
    color-scheme: light dark;
    --border-color: #d0d7de;
    --background-color: #f6f8fa;
    --border-bottom-color: #d0d7de;
    --text-color: #1f2328;
}
@media (prefers-color-scheme: dark) {
    :root {
        --border-color: #30363d;
        --background-color: #0d1117;
        --border-bottom-color: #21262d;
        --text-color: #e6edf3;
    }
    body {
        background-color: var(--background-color);
        color: var(--text-color);
    }
}

.file {
    background-color: #ffffff;
    border: 1px solid var(--border-color);
    border-radius: 6px;
    font-family: ui-monospace, SFMono-Regular, SF Mono, Menlo, monospace;
    font-size: 12px;
    margin: 1em 0;
    position: relative;
}
div.file.empty {
    overflow-wrap: anywhere;
    padding: 0.5em 0;
}
div.file:not(.empty), details.file[open] {
    overflow-x: auto;
}
.file-note {
    color: #57606a;
    font-family: -apple-system, BlinkMacSystemFont, sans-serif;
    padding: 0.25em 1.5em;
}
@media (prefers-color-scheme: dark) {
    .file {
        background-color: #161b22;
    }
    .file-note {
        color: #848d97;
    }
}

summary {
    cursor: pointer;
    padding: 0.5em 0;
    position: sticky;
    left: 0;
}

h1 {
    color: var(--text-color);
    font-family: -apple-system, BlinkMacSystemFont, sans-serif;
    font-size: 1.1em;
    font-weight: 600;
    margin-left: 0.5em;
    display: inline;
    width: 100%;
    padding: 0.5em;
}

.search {
    background-color: var(--background-color);
    border-bottom: 1px solid var(--border-color);
    font-family: -apple-system, BlinkMacSystemFont, sans-serif;
    padding: 0.5em;
    position: sticky;
    top: 0;
    z-index: 2;
}
.search input {
    width: 30em;
}
.search-status {
    color: #57606a;
    margin-left: 1em;
}
::highlight(search) {
    background-color: #fff8c5;
}
::highlight(search-current) {
    background-color: #ffd33d;
}
@media (prefers-color-scheme: dark) {
    ::highlight(search) {
        background-color: #4d3d00;
    }
    ::highlight(search-current) {
        background-color: #9e6a03;
    }
}

.section {
    background-color: var(--background-color);
    border: solid var(--border-color);
    border-width: 1px 0px;
    display: block;
    min-width: 100%;
    width: max-content;
}

.line, .section-header {
    display: flex;
    white-space: nowrap;
    width: 100%;
}

.original, .edited {
    border-bottom: 1px solid var(--border-bottom-color);
    border-right: 1px solid var(--border-color);
    color: #57606a;
    display: block;
    flex-shrink: 0;
    padding: 1px 10px 0px 0px;
    text-align: right;
    width: 3em;
    background-color: #f6f8fa;
    user-select: none;
    position: sticky;
    left: 0;
    z-index: 1;
}
.original::before {
    content: attr(data-line);
}
.edited::before {
    content: attr(data-line);
}
.edited {
    left: 3em;
}
@media (prefers-color-scheme: dark) {
    .original, .edited {
        color: #848d97;
        background-color: #161b22;
        border-right-color: #21262d;
    }
}

pre, .text {
    padding-left: 5px;
}

.text {
    white-space: pre;
    text-decoration: none;
    flex: 1;
}
.add {
    background-color: #d1f0d8;
}
.remove {
    background-color: #f8d0d0;
}
@media (prefers-color-scheme: dark) {
    .add {
        background-color: #1a4d2e;
    }
    .remove {
        background-color: #5a1e1e;
    }
}

.context {
    border-right: none;
    color: #57606a;
    background-color: #eaeef2;
    border-bottom: none;
}

@media (prefers-color-scheme: dark) {
    .context {
        color: #848d97;
        background-color: #1c2128;
    }
}

.conflict {
    background-color: #fff3cd;
    font-weight: bold;
}
.conflict-ours {
    background-color: #fde0c0;
}
.conflict-theirs {
    background-color: #ede0f5;
}
@media (prefers-color-scheme: dark) {
    .conflict {
        background-color: #3d2e00;
    }
    .conflict-ours {
        background-color: #5a2d00;
    }
    .conflict-theirs {
        background-color: #3d1f6e;
    }
}
</style>
<script>
var MAX_EXPANDED_FILES = 20;
var MAX_EXPANDED_MATCHES = 50;

var search = {
    query: '',
    files: [],
    byElement: new Map(),
    fileIndex: -1,
    matchIndex: -1,
};
var lowercaseCache = new WeakMap();

function deferredContents(details) {
    return details.querySelector(':scope > script[type="text/html"]');
}

function inflate(details) {
    var deferred = deferredContents(details);
    if (!deferred)
        return;
    deferred.insertAdjacentHTML('afterend', deferred.textContent);
    deferred.remove();
}

document.addEventListener('toggle', function(event) {
    var details = event.target;
    if (!details.open || !details.classList || !details.classList.contains('file'))
        return;
    inflate(details);

    var file = search.byElement.get(details);
    if (file && !file.ranges) {
        computeMatches(file);
        updateHighlights();
        updateStatus();
    }
}, true);

// Must match Python's html.escape(), which produced the deferred contents.
function escapeHTML(string) {
    return string.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#x27;');
}

function countOccurrences(text, query) {
    var count = 0;
    for (var index = text.indexOf(query); index >= 0; index = text.indexOf(query, index + query.length))
        ++count;
    return count;
}

// Estimates matches in a file whose contents have not been inflated by searching the escaped markup
// directly, ignoring matches inside of tags. Exact counts are computed once the file is inflated.
function countDeferredMatches(details, query) {
    var count = countOccurrences(details.querySelector(':scope > summary').textContent.toLowerCase(), query);
    var deferred = deferredContents(details);
    var html = lowercaseCache.get(deferred);
    if (html === undefined) {
        html = deferred.textContent.toLowerCase();
        lowercaseCache.set(deferred, html);
    }
    var escaped = escapeHTML(query);
    for (var index = html.indexOf(escaped); index >= 0; index = html.indexOf(escaped, index + escaped.length)) {
        if (html.lastIndexOf('>', index) > html.lastIndexOf('<', index))
            ++count;
    }
    return count;
}

function rangesFor(element, query) {
    var ranges = [];
    var walker = document.createTreeWalker(element, NodeFilter.SHOW_TEXT);
    for (var node = walker.nextNode(); node; node = walker.nextNode()) {
        var text = node.data.toLowerCase();
        for (var index = text.indexOf(query); index >= 0; index = text.indexOf(query, index + query.length)) {
            var range = new Range();
            range.setStart(node, index);
            range.setEnd(node, index + query.length);
            ranges.push(range);
        }
    }
    return ranges;
}

function computeMatches(file) {
    if (deferredContents(file.element)) {
        file.ranges = null;
        file.count = countDeferredMatches(file.element, search.query);
        return;
    }
    file.ranges = rangesFor(file.element, search.query);
    file.count = file.ranges.length;
}

function materialize(file) {
    if (file.ranges)
        return;
    inflate(file.element);
    file.element.open = true;
    computeMatches(file);
    updateHighlights();
}

function updateHighlights() {
    if (!window.CSS || !CSS.highlights)
        return;
    var highlight = new Highlight();
    for (var file of search.files) {
        for (var range of file.ranges || [])
            highlight.add(range);
    }
    CSS.highlights.set('search', highlight);

    var current = search.files[search.fileIndex];
    if (current && current.ranges && current.ranges[search.matchIndex]) {
        var currentHighlight = new Highlight(current.ranges[search.matchIndex]);
        currentHighlight.priority = 1;
        CSS.highlights.set('search-current', currentHighlight);
    } else
        CSS.highlights.delete('search-current');
}

function updateStatus() {
    var status = document.querySelector('.search-status');
    if (!search.query) {
        status.textContent = '';
        return;
    }
    var total = 0;
    var position = 0;
    for (var index = 0; index < search.files.length; ++index) {
        if (index < search.fileIndex)
            position += search.files[index].count;
        total += search.files[index].count;
    }
    if (!total) {
        status.textContent = 'No matches';
        return;
    }
    var files = search.files.length + (search.files.length === 1 ? ' file' : ' files');
    if (search.fileIndex < 0)
        status.textContent = total + ' matches in ' + files;
    else
        status.textContent = (position + search.matchIndex + 1) + ' of ' + total + ' matches in ' + files;
}

function select(fileIndex, matchIndex) {
    search.fileIndex = fileIndex;
    search.matchIndex = matchIndex;
    var file = search.files[fileIndex];
    file.element.open = true;
    updateHighlights();
    updateStatus();
    file.ranges[matchIndex].startContainer.parentElement.scrollIntoView({ block: 'center', inline: 'nearest' });
}

function step(direction) {
    var count = search.files.length;
    if (!count)
        return;
    var fileIndex = search.fileIndex < 0 ? (direction > 0 ? -1 : 0) : search.fileIndex;
    var matchIndex = search.fileIndex < 0 ? -1 : search.matchIndex + direction;
    for (var attempts = 0; attempts <= count; ++attempts) {
        var file = search.files[fileIndex];
        if (file && file.ranges && matchIndex >= 0 && matchIndex < file.ranges.length) {
            select(fileIndex, matchIndex);
            return;
        }
        fileIndex = (fileIndex + direction + count) % count;
        file = search.files[fileIndex];
        materialize(file);
        matchIndex = direction > 0 ? 0 : file.ranges.length - 1;
    }
}

function runSearch(query) {
    search.query = query.toLowerCase();
    search.files = [];
    search.byElement = new Map();
    search.fileIndex = -1;
    search.matchIndex = -1;

    for (var element of document.querySelectorAll('div.file.commit, details.file, div.file.empty')) {
        if (!search.query) {
            element.hidden = false;
            continue;
        }
        var file = { element: element, count: 0, ranges: null };
        computeMatches(file);
        // Commit messages stay visible as context for the files around them.
        if (!element.classList.contains('commit'))
            element.hidden = !file.count;
        if (!file.count)
            continue;
        search.files.push(file);
        search.byElement.set(element, file);
        if (element.tagName === 'DETAILS' && search.files.length <= MAX_EXPANDED_MATCHES)
            materialize(file);
    }

    updateHighlights();
    updateStatus();
    if (search.query)
        step(1);
}

document.addEventListener('DOMContentLoaded', function() {
    var files = document.querySelectorAll('details.file');
    if (files.length <= MAX_EXPANDED_FILES) {
        for (var file of files)
            file.open = true;
    }

    var input = document.querySelector('.search input');
    var timer = null;

    // Steps to the next match, unless the query has changed since the last search.
    function advance(direction) {
        clearTimeout(timer);
        if (input.value.toLowerCase() !== search.query)
            runSearch(input.value);
        else
            step(direction);
    }

    input.addEventListener('input', function() {
        clearTimeout(timer);
        timer = setTimeout(function() { runSearch(input.value); }, 300);
    });
    input.addEventListener('keydown', function(event) {
        if (event.key === 'Enter') {
            event.preventDefault();
            advance(event.shiftKey ? -1 : 1);
        } else if (event.key === 'Escape')
            input.blur();
    });

    // The browser's find only sees expanded files, so Cmd-F uses our search, which covers every file.
    document.addEventListener('keydown', function(event) {
        if (!(event.metaKey || event.ctrlKey) || event.altKey)
            return;
        var key = event.key.toLowerCase();
        if (key === 'f') {
            event.preventDefault();
            input.focus();
            input.select();
        } else if (key === 'g' && input.value) {
            event.preventDefault();
            advance(event.shiftKey ? -1 : 1);
        }
    });
});
</script>
</head>
<body>
<div class='search'>
    <input type='search' placeholder='Search all files'>
    <span class='search-status'></span>
</div>
'''
    TAIL = '</body>\n</html>\n'
    INDENT = 4 * ' '
    FILES_CHANGED_RE = re.compile(r'^ (\d+ files? changed)?(create mode\s+)?')
    name = 'html'

    class Type(object):
        DELETED = 0
        MODIFIED = 1
        CREATED = 2
        MOVED = 3

    class Section(object):
        SECTION_RE = re.compile(r'@@\s+-(\d+)(?:,(\d+))?\s+\+(\d+)(?:,(\d+))?\s+@@\s*(.*)')
        ORIGINAL = 1
        EDITED = 2
        CONFLICT = 3
        CONFLICT_OURS = 4
        CONFLICT_THEIRS = 5

        @classmethod
        def from_header(cls, string: str) -> HTMLDiff.Section | None:
            match = cls.SECTION_RE.match(string)
            if not match:
                return None
            return cls(
                title=match.group(5),
                original_position=match.group(1),
                edited_position=match.group(3),
                original_length=match.group(2) or 1,
                edited_length=match.group(4) or 1,
            )

        def __init__(
            self, title: str,
            original_position: int | str, edited_position: int | str,
            original_length: int | str = 0, edited_length: int | str = 0,
        ) -> None:
            self.title = title
            self.original_position = int(original_position)
            self.edited_position = int(edited_position)
            self.original_remaining = int(original_length)
            self.edited_remaining = int(edited_length)

        @property
        def complete(self) -> bool:
            return self.original_remaining <= 0 and self.edited_remaining <= 0

        def consume(self, prefix: str) -> None:
            if prefix != '+':
                self.original_remaining -= 1
            if prefix != '-':
                self.edited_remaining -= 1

        def header(self) -> list[str]:
            return [
                '<div class="section-header context">',
                '    <span class="original context">@</span>',
                '    <span class="edited context">@</span>',
                '    <span class="section-title">{}</span>'.format(escape(self.title)),
                '</div>',
            ]

        def line(self, line: str, type: int | None = None) -> list[str]:
            type_classes: dict[int | None, str] = {
                self.EDITED: ' add',
                self.ORIGINAL: ' remove',
                self.CONFLICT: ' conflict',
                self.CONFLICT_OURS: ' conflict-ours',
                self.CONFLICT_THEIRS: ' conflict-theirs',
            }
            type_class = type_classes.get(type, '')
            line_classes: dict[int | None, str] = {
                self.CONFLICT: ' context',
            }
            line_class = line_classes.get(type, '')

            orig = self.original_position if type in (None, self.ORIGINAL, self.CONFLICT_OURS) else None
            edit = self.edited_position if type in (None, self.EDITED, self.CONFLICT_THEIRS) else None
            result = [
                '<div class="line{}">'.format(type_class),
                '    <span class="original{}"{}></span>'.format(line_class, ' data-line="{}"'.format(orig) if orig is not None else ''),
                '    <span class="edited{}"{}></span>'.format(line_class, ' data-line="{}"'.format(edit) if edit is not None else ''),
                '    <span class="text{}">{}</span>'.format(type_class, escape(line)),
                '</div>',
            ]
            if type is not self.EDITED and type is not self.CONFLICT_THEIRS:
                self.original_position += 1
            if type is not self.ORIGINAL and type is not self.CONFLICT_OURS:
                self.edited_position += 1
            return result

    @classmethod
    def title_for(cls, *paths: str) -> str | None:
        files = list(paths)
        prefix = ['a/', 'b/']
        for index in range(len(prefix)):
            if not files or len(files) <= index:
                break
            if files[index].startswith(prefix[index]):
                files[index] = files[index][len(prefix[index]):]

        for empty in ['/dev/null']:
            while empty in files or []:
                files.remove(empty)

        if not files:
            return None
        if len(files) == 1:
            return files[0]

        if files[0] == files[1]:
            return files[0]
        return '{} -> {}'.format(*files)

    @classmethod
    def paths_from_diff_header(cls, args: str) -> list[str]:
        args = args.rstrip()
        if not args.startswith('--git '):
            return args.split()[-1:]
        args = args[len('--git '):]

        # Paths may contain spaces, so prefer the split where both sides name the same file
        half = len(args) // 2
        if args[half:half + 1] == ' ' and args[2:half] == args[half + 3:]:
            return [args[:half], args[half + 1:]]
        if ' b/' in args:
            original, edited = args.split(' b/', 1)
            return [original, 'b/' + edited]
        return args.split()

    def commit_message_line(self, line: str, context: str | None = None) -> list[str]:
        if context:
            return [
                '<div class="section-header context">',
                '    <span class="original context">{}</span>'.format(escape(context)),
                '    <span class="section-title">{}</span>'.format(escape(line.rstrip())),
                '</div>',
            ]
        result = [
            '<div class="line">',
            '    <span class="original" data-line="{}"></span>'.format(self._position),
            '    <span class="text">{}</span>'.format(escape(line.rstrip())),
            '</div>',
        ]
        self._position += 1
        return result

    def __init__(self, **kwargs: Any) -> None:
        super(HTMLDiff, self).__init__(**kwargs)

        if self.block is None:
            self.block = False
        self.accepted = True

        self._file_handle: IO[str] | None = None
        self._div: list[tuple[str | None, str]] = []
        self._section: HTMLDiff.Section | None = None
        self._current_title: str | None = None
        self._pending_title: str | None = None
        self._pending_notes: list[str] = []
        self._file_status: str | None = None
        self._old_mode: str | None = None
        self._files: list[str] = []
        self._conflict_side: str | None = None
        self._in_commit_message = False
        self._skip_until_header = False
        self._position = 1

        self.file = os.path.join(tempfile.gettempdir(), 'diff.html')

    def _start_div(self, klass: str | None = None, extra_class: str | None = None) -> None:
        assert self._file_handle is not None
        self._file_handle.write("{}<div class='{}'>\n".format(len(self._div) * self.INDENT, '{} {}'.format(klass, extra_class) if extra_class else klass))
        self._div.append((klass, '</div>'))

    def _start_collapsed_file(self, title: str | None) -> None:
        # File contents are deferred inside an inert script block so the browser does not build their DOM
        # until the user expands the file. Contents are escaped, so they cannot contain a closing script tag.
        assert self._file_handle is not None
        indent = len(self._div) * self.INDENT
        self._file_handle.write("{}<details class='file'>\n".format(indent))
        self._file_handle.write('{}{}<summary><h1>{}</h1></summary>\n'.format(indent, self.INDENT, escape(title or '')))
        self._file_handle.write("{}{}<script type='text/html'>\n".format(indent, self.INDENT))
        self._div.append(('file', '{}</script>\n{}</details>'.format(self.INDENT, indent)))

    def _flush_pending_file(self) -> None:
        # Files without any sections (pure moves, copies, binary or mode changes) have nothing to expand.
        if self._pending_title is None:
            return
        assert self._file_handle is not None
        indent = len(self._div) * self.INDENT
        self._file_handle.write("{}<div class='file empty'>\n".format(indent))
        self._file_handle.write('{}{}<h1>{}</h1>\n'.format(indent, self.INDENT, escape(self._pending_title)))
        self._write_notes(indent + self.INDENT)
        self._file_handle.write('{}</div>\n'.format(indent))
        self._pending_title = None

    def _add_note(self, note: str) -> None:
        # Notes describe changes without sections, they can only be attached to a file which hasn't been written yet
        if self._pending_title is not None:
            self._pending_notes.append(note)

    def _write_notes(self, indent: str) -> None:
        assert self._file_handle is not None
        for note in self._pending_notes:
            self._file_handle.write("{}<div class='file-note'>{}</div>\n".format(indent, escape(note)))
        self._pending_notes = []

    def _set_title(self, title: str | None) -> None:
        if title is None or title == self._current_title:
            return
        self._flush_pending_file()
        self._end_div(klass='file')
        self._pending_title = title
        self._current_title = title

    def _end_div(self, klass: str | None = None) -> None:
        if not self._div or self._div[-1][0] != klass:
            return
        assert self._file_handle is not None
        _, closing = self._div.pop()
        self._file_handle.write("{}{}\n".format(len(self._div) * self.INDENT, closing))

    def add_line(self, line: str | None) -> str:
        line = super(HTMLDiff, self).add_line(line)
        # local.Git.diff strips trailing whitespace from the signature delimiter
        is_signature = line.rstrip() == '--'
        if self._skip_until_header:
            if not is_signature and not line.startswith(('diff ', 'From ')):
                return line
            self._skip_until_header = False
        if line.rstrip() == 'GIT binary patch':
            # Binary patches are base85 encoded data, which cannot be meaningfully displayed
            self._skip_until_header = True
            self._add_note('Binary file {}'.format(self._file_status or 'changed'))
            return line
        if is_signature and not self._in_commit_message and (not self._section or self._section.complete):
            # format-patch ends each patch with a signature (usually the git version), which is not part of the diff
            self._end_div(klass='section')
            self._section = None
            self._skip_until_header = True
            return line

        # Indented lines are always section content, never headers
        splitline = [] if line[:1].isspace() else line.rstrip().split(maxsplit=1)
        if len(splitline) == 2 and splitline[0] in ('rename', 'copy', 'new', 'deleted', 'old', 'Binary'):
            splitline = line.rstrip().split(maxsplit=2)
            splitline[0] = '{} {}'.format(splitline[0], splitline.pop(1))
        word = splitline[0] if splitline else None

        if word == '---' and self._in_commit_message:
            self._end_div(klass='file')
            self._in_commit_message = False
            return line

        if self._in_commit_message:
            if word == 'From:' and len(splitline) > 1:
                output_lines = self.commit_message_line(splitline[1], context='By')
            elif word == 'Date:' and len(splitline) > 1:
                output_lines = self.commit_message_line(splitline[1], context='Date')
            else:
                output_lines = self.commit_message_line(line or ' ')
            assert self._file_handle is not None
            for output_line in output_lines:
                self._file_handle.write('{}{}\n'.format(len(self._div) * self.INDENT, output_line))
            return line

        if word == 'From':
            self._flush_pending_file()
            self._in_commit_message = True
            self._end_div(klass='section')
            self._end_div(klass='file')
            self._start_div(klass='file', extra_class='commit')
            assert self._file_handle is not None
            self._file_handle.write('{}<h1>{}</h1>\n'.format(len(self._div) * self.INDENT, escape(splitline[1]) if len(splitline) > 1 else '?'))
            self._start_div(klass='section')
            self._position = 1
            self._section = None
            self._files = []
            self._current_title = None
            self._file_status = None
            return line

        if word in ('diff', 'index', 'deleted', 'new file', 'deleted file'):
            self._files = []
            self._end_div(klass='section')
            self._section = None
            if word == 'diff':
                self._file_status = None
                self._old_mode = None
                # Binary and mode-only changes have no ---/+++ lines, the diff header is their only title
                self._set_title(self.title_for(*self.paths_from_diff_header(splitline[1] if len(splitline) > 1 else '')))
            elif word == 'new file':
                self._file_status = 'added'
            elif word == 'deleted file':
                self._file_status = 'deleted'
            return line

        if word == 'old mode' and len(splitline) > 1:
            self._old_mode = splitline[1]
            return line
        if word == 'new mode' and len(splitline) > 1:
            self._add_note('Mode changed from {} to {}'.format(self._old_mode or '?', splitline[1]))
            return line
        if word == 'Binary files':
            self._add_note('Binary file {}'.format(self._file_status or 'changed'))
            return line
        if word in ('similarity', 'dissimilarity'):
            return line

        if not self._files and word in ('---', 'rename from', 'copy from') and len(splitline) > 1:
            self._files = [splitline[1]]
            return line
        if len(self._files) == 1 and word in ('+++', 'rename to', 'copy to'):
            self._files.append(splitline[1])
            title = self.title_for(*self._files)
            if self._pending_title is not None and title is not None:
                # Prefer these paths over the title parsed from the diff header, which can be ambiguous
                self._pending_title = title
                self._current_title = title
            else:
                self._set_title(title)
            return line

        new_section = self.Section.from_header(line)
        if new_section:
            assert self._file_handle is not None
            if self._pending_title is not None:
                self._start_collapsed_file(self._pending_title)
                self._write_notes(len(self._div) * self.INDENT)
                self._pending_title = None
            if self._section:
                self._end_div(klass='section')
                self._file_handle.write('{}<br>\n'.format(len(self._div) * self.INDENT))
            self._start_div(klass='section')
            self._conflict_side = None
            self._section = new_section
            for header_line in self._section.header():
                self._file_handle.write('{}{}\n'.format(len(self._div) * self.INDENT, header_line))
            return line

        if not self._section:
            stripped_line = line.rstrip()
            if not stripped_line:
                return line
            if self.ADD_SUB_RE.match(stripped_line):
                return line
            if self.FILES_CHANGED_RE.match(stripped_line):
                return line

            sys.stderr.write('Unrecognized diff format, excluding:\n')
            sys.stderr.write(line)
            return line

        assert self._file_handle is not None
        typ: int | None = None
        if line.startswith('+<<<'):
            self._conflict_side = 'ours'
            typ = self._section.CONFLICT
        elif line.startswith('+==='):
            self._conflict_side = 'theirs'
            typ = self._section.CONFLICT
        elif line.startswith('+>>>'):
            self._conflict_side = None
            typ = self._section.CONFLICT
        elif self._conflict_side == 'ours':
            typ = self._section.CONFLICT_OURS
        elif self._conflict_side == 'theirs':
            typ = self._section.CONFLICT_THEIRS
        elif line[0] == '-':
            typ = self._section.ORIGINAL
        elif line[0] == '+':
            typ = self._section.EDITED

        # '\ No newline at end of file' annotates the previous line, it is not counted in the hunk
        if line[0] != '\\':
            self._section.consume(line[0])
        for output_line in self._section.line(line[1:-1], type=typ):
            self._file_handle.write('{}{}\n'.format(len(self._div) * self.INDENT, output_line))

        return line

    def __enter__(self) -> HTMLDiff:
        self._file_handle = open(self.file, 'w')
        self._div = []
        self._section = None
        self._current_title = None
        self._pending_title = None
        self._pending_notes = []
        self._file_status = None
        self._old_mode = None
        self._files = []
        self._skip_until_header = False

        self._file_handle.write(self.HEAD)

        return self

    def __exit__(self, *args: Any, **kwargs: Any) -> None:
        if not self._file_handle:
            return

        self._flush_pending_file()
        while self._div:
            self._end_div(klass=self._div[-1][0])
        self._file_handle.write(self.TAIL)

        self._file_handle.close()
        self._file_handle = None

        if self.block:
            finished = threading.Event()
            diff_file = self.file

            class _Handler(BaseHTTPRequestHandler):
                def do_GET(self) -> None:
                    try:
                        with open(diff_file, 'rb') as f:
                            content = f.read()
                        script = (
                            b'<script>window.addEventListener("beforeunload",function(){'
                            b'try{navigator.sendBeacon("/done")}catch(e){}});</script>'
                        )
                        content = content.replace(b'</body>', script + b'</body>', 1)
                        self.send_response(200)
                        self.send_header('Content-Type', 'text/html; charset=utf-8')
                        self.send_header('Content-Length', str(len(content)))
                        self.end_headers()
                        self.wfile.write(content)
                    except Exception:
                        self.send_error(500)

                def do_POST(self) -> None:
                    self.send_response(204)
                    self.end_headers()
                    if self.path == '/done':
                        finished.set()

                def log_message(self, *args: Any, **kwargs: Any) -> None:
                    pass

            server = HTTPServer(('127.0.0.1', 0), _Handler)
            port = server.server_address[1]
            server_thread = threading.Thread(target=server.serve_forever)
            server_thread.daemon = True
            server_thread.start()

            def _wait_for_enter() -> None:
                try:
                    if sys.stdin.readline().strip().lower() in ('n', 'no'):
                        self.accepted = False
                finally:
                    finished.set()

            enter_thread = threading.Thread(target=_wait_for_enter)
            enter_thread.daemon = True
            enter_thread.start()

            Terminal.open_url('http://127.0.0.1:{}/'.format(port))
            finished.wait()
            server.shutdown()
        else:
            Terminal.open_url('file://{}'.format(self.file))
