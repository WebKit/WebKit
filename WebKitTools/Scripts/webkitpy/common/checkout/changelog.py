# Copyright (C) 2009, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# WebKit's Python module for parsing and modifying ChangeLog files

import codecs
import fileinput # inplace file editing for set_reviewer_in_changelog
import os.path
import re
import textwrap

from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.config.committers import CommitterList
from webkitpy.common.net.bugzilla import parse_bug_id


def view_source_url(revision_number):
    # FIMXE: This doesn't really belong in this file, but we don't have a
    # better home for it yet.
    # Maybe eventually a webkit_config.py?
    return "http://trac.webkit.org/changeset/%s" % revision_number


class ChangeLogEntry(object):
    # e.g. 2009-06-03  Eric Seidel  <eric@webkit.org>
    date_line_regexp = r'^(?P<date>\d{4}-\d{2}-\d{2})\s+(?P<name>.+?)\s+<(?P<email>[^<>]+)>$'

    def __init__(self, contents, committer_list=CommitterList()):
        self._contents = contents
        self._committer_list = committer_list
        self._parse_entry()

    def _parse_entry(self):
        match = re.match(self.date_line_regexp, self._contents, re.MULTILINE)
        if not match:
            log("WARNING: Creating invalid ChangeLogEntry:\n%s" % self._contents)

        # FIXME: group("name") does not seem to be Unicode?  Probably due to self._contents not being unicode.
        self._author_name = match.group("name") if match else None
        self._author_email = match.group("email") if match else None

        match = re.search("^\s+Reviewed by (?P<reviewer>.*?)[\.,]?\s*$", self._contents, re.MULTILINE) # Discard everything after the first period
        self._reviewer_text = match.group("reviewer") if match else None

        self._reviewer = self._committer_list.committer_by_name(self._reviewer_text)
        self._author = self._committer_list.committer_by_email(self._author_email) or self._committer_list.committer_by_name(self._author_name)

    def author_name(self):
        return self._author_name

    def author_email(self):
        return self._author_email

    def author(self):
        return self._author # Might be None

    # FIXME: Eventually we would like to map reviwer names to reviewer objects.
    # See https://bugs.webkit.org/show_bug.cgi?id=26533
    def reviewer_text(self):
        return self._reviewer_text

    def reviewer(self):
        return self._reviewer # Might be None

    def contents(self):
        return self._contents

    def bug_id(self):
        return parse_bug_id(self._contents)


# FIXME: Various methods on ChangeLog should move into ChangeLogEntry instead.
class ChangeLog(object):

    def __init__(self, path):
        self.path = path

    _changelog_indent = " " * 8

    @staticmethod
    def parse_latest_entry_from_file(changelog_file):
        """changelog_file must be a file-like object which returns
        unicode strings.  Use codecs.open or StringIO(unicode())
        to pass file objects to this class."""
        date_line_regexp = re.compile(ChangeLogEntry.date_line_regexp)
        entry_lines = []
        # The first line should be a date line.
        first_line = changelog_file.readline()
        assert(isinstance(first_line, unicode))
        if not date_line_regexp.match(first_line):
            return None
        entry_lines.append(first_line)

        for line in changelog_file:
            # If we've hit the next entry, return.
            if date_line_regexp.match(line):
                # Remove the extra newline at the end
                return ChangeLogEntry(''.join(entry_lines[:-1]))
            entry_lines.append(line)
        return None # We never found a date line!

    def latest_entry(self):
        # ChangeLog files are always UTF-8, we read them in as such to support Reviewers with unicode in their names.
        changelog_file = codecs.open(self.path, "r", "utf-8")
        try:
            return self.parse_latest_entry_from_file(changelog_file)
        finally:
            changelog_file.close()

    # _wrap_line and _wrap_lines exist to work around
    # http://bugs.python.org/issue1859

    def _wrap_line(self, line):
        return textwrap.fill(line,
                             width=70,
                             initial_indent=self._changelog_indent,
                             # Don't break urls which may be longer than width.
                             break_long_words=False,
                             subsequent_indent=self._changelog_indent)

    # Workaround as suggested by guido in
    # http://bugs.python.org/issue1859#msg60040

    def _wrap_lines(self, message):
        lines = [self._wrap_line(line) for line in message.splitlines()]
        return "\n".join(lines)

    # This probably does not belong in changelogs.py
    def _message_for_revert(self, revision, reason, bug_url):
        message = "Unreviewed, rolling out r%s.\n" % revision
        message += "%s\n" % view_source_url(revision)
        if bug_url:
            message += "%s\n" % bug_url
        # Add an extra new line after the rollout links, before any reason.
        message += "\n"
        if reason:
            message += "%s\n\n" % reason
        return self._wrap_lines(message)

    def update_for_revert(self, revision, reason, bug_url=None):
        reviewed_by_regexp = re.compile(
                "%sReviewed by NOBODY \(OOPS!\)\." % self._changelog_indent)
        removing_boilerplate = False
        # inplace=1 creates a backup file and re-directs stdout to the file
        for line in fileinput.FileInput(self.path, inplace=1):
            if reviewed_by_regexp.search(line):
                message_lines = self._message_for_revert(revision,
                                                         reason,
                                                         bug_url)
                print reviewed_by_regexp.sub(message_lines, line),
                # Remove all the ChangeLog boilerplate between the Reviewed by
                # line and the first changed file.
                removing_boilerplate = True
            elif removing_boilerplate:
                if line.find('*') >= 0: # each changed file is preceded by a *
                    removing_boilerplate = False

            if not removing_boilerplate:
                print line,

    def set_reviewer(self, reviewer):
        # inplace=1 creates a backup file and re-directs stdout to the file
        for line in fileinput.FileInput(self.path, inplace=1):
            # Trailing comma suppresses printing newline
            print line.replace("NOBODY (OOPS!)", reviewer.encode("utf-8")),

    def set_short_description_and_bug_url(self, short_description, bug_url):
        message = "%s\n        %s" % (short_description, bug_url)
        for line in fileinput.FileInput(self.path, inplace=1):
            print line.replace("Need a short description and bug URL (OOPS!)", message.encode("utf-8")),
