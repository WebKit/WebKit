# Copyright (c) 2012 Google Inc. All rights reserved.
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

from optparse import make_option

from webkitpy.common.net.omahaproxy import OmahaProxy
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand

import re


class ChromeChannels(AbstractDeclarativeCommand):
    name = "chrome-channels"
    help_text = "List which chrome channels include the patches in bugs returned by QUERY."
    argument_names = "QUERY"
    long_help = """Retrieves the current list of Chrome releases from omahaproxy.appspot.com,
and then runs the bugzilla quicksearch QUERY on bugs.bugzilla.org. For each bug
returned by query, a single svn commit is deduced, and a short summary is
printed of each bug listing which Chrome channels contain each bugs associated
commit.

The QUERY can be as simple as a bug number, or a comma delimited list of bug
numbers. See https://bugzilla.mozilla.org/page.cgi?id=quicksearch.html for full
documentation on the query format."""

    chrome_channels = OmahaProxy.chrome_channels
    commited_pattern = "Committed r([0-9]+): <http://trac.webkit.org/changeset/\\1>"
    rollout_pattern = "Rolled out in http://trac.webkit.org/changeset/[0-9]+"

    def __init__(self):
        AbstractDeclarativeCommand.__init__(self)
        self._re_committed = re.compile(self.commited_pattern)
        self._re_rollout = re.compile(self.rollout_pattern)
        self._omahaproxy = OmahaProxy()

    def _channels_for_bug(self, revisions, bug):
        comments = bug.comments()
        commit = None

        # Scan the comments, looking for a sane list of commits and rollbacks.
        for comment in comments:
            commit_match = self._re_committed.search(comment['text'])
            if commit_match:
                if commit:
                    return "%5s %s\n... has too confusing a commit history to parse, skipping\n" % (bug.id(), bug.title())
                commit = int(commit_match.group(1))
            if self._re_rollout.search(comment['text']):
                commit = None
        if not commit:
            return "%5s %s\n... does not appear to have an associated commit.\n" % (bug.id(), bug.title())

        # We now know that we have a commit, so gather up the list of platforms
        # by channel, then print.
        by_channel = {}
        for revision in revisions:
            channel = revision['channel']
            if revision['commit'] < commit:
                continue
            if not channel in by_channel:
                by_channel[revision['channel']] = " %6s:" % channel
            by_channel[channel] += " %s," % revision['platform']
        if not by_channel:
            return "%5s %s (r%d)\n... not yet released in any Chrome channels.\n" % (bug.id(), bug.title(), commit)
        retval = "%5s %s (r%d)\n" % (bug.id(), bug.title(), commit)
        for channel in self.chrome_channels:
            if channel in by_channel:
                retval += by_channel[channel][:-1]
                retval += "\n"
        return retval

    def execute(self, options, args, tool):
        search_string = args[0]
        revisions = self._omahaproxy.get_revisions()
        bugs = tool.bugs.queries.fetch_bugs_matching_quicksearch(search_string)
        if not bugs:
            print "No bugs found matching '%s'" % search_string
            return
        for bug in bugs:
            print self._channels_for_bug(revisions, bug),
