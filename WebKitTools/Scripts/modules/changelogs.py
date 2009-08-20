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

import fileinput # inplace file editing for set_reviewer_in_changelog
import re

# FIMXE: This doesn't really belong in this file, but we don't have a better home for it yet.
# Maybe eventually a webkit_config.py?
def view_source_url(revision_number):
    return "http://trac.webkit.org/changeset/%s" % revision_number


class ChangeLog:
    def __init__(self, path):
        self.path = path

    # e.g. 2009-06-03  Eric Seidel  <eric@webkit.org>
    date_line_regexp = re.compile('^(\d{4}-\d{2}-\d{2})' # Consume the date.
                                + '\s+(.+)\s+' # Consume the name.
                                + '<([^<>]+)>$') # And finally the email address.

    @staticmethod
    def _parse_latest_entry_from_file(changelog_file):
        entry_lines = []
        # The first line should be a date line.
        first_line = changelog_file.readline()
        if not ChangeLog.date_line_regexp.match(first_line):
            return None
        entry_lines.append(first_line)

        for line in changelog_file:
            # If we've hit the next entry, return.
            if ChangeLog.date_line_regexp.match(line):
                return ''.join(entry_lines[:-1]) # Remove the extra newline at the end
            entry_lines.append(line)
        return None # We never found a date line!

    def latest_entry(self):
        changelog_file = open(self.path)
        try:
            return self._parse_latest_entry_from_file(changelog_file)
        finally:
            changelog_file.close()

    def update_for_revert(self, revision):
        reviewed_by_regexp = re.compile('Reviewed by NOBODY \(OOPS!\)\.')
        removing_boilerplate = False
        # inplace=1 creates a backup file and re-directs stdout to the file
        for line in fileinput.FileInput(self.path, inplace=1):
            if reviewed_by_regexp.search(line):
                print reviewed_by_regexp.sub("No review, rolling out r%s." % revision, line),
                print "        %s\n" % view_source_url(revision)
                # Remove all the ChangeLog boilerplate between the Reviewed by line and the first changed file.
                removing_boilerplate = True
            elif removing_boilerplate:
                if line.find('*') >= 0 : # each changed file is preceded by a *
                    removing_boilerplate = False

            if not removing_boilerplate:
                print line,

    def set_reviewer(self, reviewer):
        # inplace=1 creates a backup file and re-directs stdout to the file
        for line in fileinput.FileInput(self.path, inplace=1):
            print line.replace("NOBODY (OOPS!)", reviewer.encode("utf-8")), # Trailing comma suppresses printing newline
