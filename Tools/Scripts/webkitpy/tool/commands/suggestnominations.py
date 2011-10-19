# Copyright (c) 2011 Google Inc. All rights reserved.
# Copyright (c) 2011 Code Aurora Forum. All rights reserved.
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
import re

from webkitpy.common.checkout.changelog import ChangeLogEntry
from webkitpy.common.config.committers import CommitterList
from webkitpy.tool import steps
from webkitpy.tool.grammar import join_with_separators
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand


class SuggestNominations(AbstractDeclarativeCommand):
    name = "suggest-nominations"
    help_text = "Suggest contributors for committer/reviewer nominations"

    def __init__(self):
        options = [
            make_option("--committer-minimum", action="store", dest="committer_minimum", type="int", default=10, help="Specify minimum patch count for Committer nominations."),
            make_option("--reviewer-minimum", action="store", dest="reviewer_minimum", type="int", default=80, help="Specify minimum patch count for Reviewer nominations."),
            make_option("--max-commit-age", action="store", dest="max_commit_age", type="int", default=9, help="Specify max commit age to consider for nominations (in months)."),
            make_option("--show-commits", action="store_true", dest="show_commits", default=False, help="Show commit history with nomination suggestions."),
        ]

        AbstractDeclarativeCommand.__init__(self, options=options)
        # FIXME: This should probably be on the tool somewhere.
        self._committer_list = CommitterList()

    _counters_by_name = {}
    _counters_by_email = {}

    def _init_options(self, options):
        self.committer_minimum = options.committer_minimum
        self.reviewer_minimum = options.reviewer_minimum
        self.max_commit_age = options.max_commit_age
        self.show_commits = options.show_commits
        self.verbose = options.verbose

    # FIXME: This should move to scm.py
    def _recent_commit_messages(self):
        git_log = self._tool.executive.run_command(['git', 'log', '--since="%s months ago"' % self.max_commit_age])
        match_git_svn_id = re.compile(r"\n\n    git-svn-id:.*\n", re.MULTILINE)
        match_get_log_lines = re.compile(r"^\S.*\n", re.MULTILINE)
        match_leading_indent = re.compile(r"^[ ]{4}", re.MULTILINE)

        messages = re.split(r"commit \w{40}", git_log)[1:]  # Ignore the first message which will be empty.
        for message in messages:
            # Remove any lines from git and unindent all the lines
            (message, _) = match_git_svn_id.subn("", message)
            (message, _) = match_get_log_lines.subn("", message)
            (message, _) = match_leading_indent.subn("", message)
            yield message.lstrip()  # Remove any leading newlines from the log message.

    # e.g. Patch by Eric Seidel <eric@webkit.org> on 2011-09-15
    patch_by_regexp = r'^Patch by (?P<name>.+?)\s+<(?P<email>[^<>]+)> on (?P<date>\d{4}-\d{2}-\d{2})$'

    def _count_recent_patches(self):
        # This entire block could be written as a map/reduce over the messages.
        for message in self._recent_commit_messages():
            # FIXME: This should use ChangeLogEntry to do the entire parse instead
            # of grabbing at its regexps.
            dateline_match = re.match(ChangeLogEntry.date_line_regexp, message, re.MULTILINE)
            if not dateline_match:
                # Modern commit messages don't just dump the ChangeLog entry, but rather
                # have a special Patch by line for non-committers.
                dateline_match = re.search(self.patch_by_regexp, message, re.MULTILINE)
                if not dateline_match:
                    continue

            author_email = dateline_match.group("email")
            if not author_email:
                continue

            # We only care about reviewed patches, so make sure it has a valid reviewer line.
            reviewer_match = re.search(ChangeLogEntry.reviewed_by_regexp, message, re.MULTILINE)
            # We might also want to validate the reviewer name against the committer list.
            if not reviewer_match or not reviewer_match.group("reviewer"):
                continue

            author_name = dateline_match.group("name")
            if not author_name:
                continue

            if re.search("([^a-zA-Z]and[^a-zA-Z])|(,)|(@)", author_name):
                # This entry seems to have multiple reviewers, or invalid characters, so reject it.
                continue

            svn_id_match = re.search(ChangeLogEntry.svn_id_regexp, message, re.MULTILINE)
            if svn_id_match:
                svn_id = svn_id_match.group("svnid")
            if not svn_id_match or not svn_id:
                svn_id = "unknown"
            commit_date = dateline_match.group("date")

            # See if we already have a contributor with this name or email
            counter_by_name = self._counters_by_name.get(author_name)
            counter_by_email = self._counters_by_email.get(author_email)
            if counter_by_name:
                if counter_by_email:
                    if counter_by_name != counter_by_email:
                        # Merge these two counters  This is for the case where we had
                        # John Smith (jsmith@gmail.com) and Jonathan Smith (jsmith@apple.com)
                        # and just found a John Smith (jsmith@apple.com).  Now we know the
                        # two names are the same person
                        counter_by_name['names'] |= counter_by_email['names']
                        counter_by_name['emails'] |= counter_by_email['emails']
                        counter_by_name['count'] += counter_by_email.get('count', 0)
                        self._counters_by_email[author_email] = counter_by_name
                else:
                    # Add email to the existing counter
                    self._counters_by_email[author_email] = counter_by_name
                    counter_by_name['emails'] |= set([author_email])
            else:
                if counter_by_email:
                    # Add name to the existing counter
                    self._counters_by_name[author_name] = counter_by_email
                    counter_by_email['names'] |= set([author_name])
                else:
                    # Create new counter
                    new_counter = {'names': set([author_name]), 'emails': set([author_email]), 'latest_name': author_name, 'latest_email': author_email, 'commits': ""}
                    self._counters_by_name[author_name] = new_counter
                    self._counters_by_email[author_email] = new_counter

            assert(self._counters_by_name[author_name] == self._counters_by_email[author_email])
            counter = self._counters_by_name[author_name]
            counter['count'] = counter.get('count', 0) + 1

            if svn_id.isdigit():
                svn_id = "http://trac.webkit.org/changeset/" + svn_id
            counter['commits'] += "  commit: %s on %s by %s (%s)\n" % (svn_id, commit_date, author_name, author_email)

        return self._counters_by_email

    def _collect_nominations(self, counters_by_email):
        nominations = []
        for author_email, counter in counters_by_email.items():
            if author_email != counter['latest_email']:
                continue
            roles = []

            contributor = self._committer_list.contributor_by_email(author_email)

            author_name = counter['latest_name']
            patch_count = counter['count']

            if patch_count >= self.committer_minimum and (not contributor or not contributor.can_commit):
                roles.append("committer")
            if patch_count >= self.reviewer_minimum  and (not contributor or not contributor.can_review):
                roles.append("reviewer")
            if roles:
                nominations.append({
                    'roles': roles,
                    'author_name': author_name,
                    'author_email': author_email,
                    'patch_count': patch_count,
                })
        return nominations

    def _print_nominations(self, nominations):
        def nomination_cmp(a_nomination, b_nomination):
            roles_result = cmp(a_nomination['roles'], b_nomination['roles'])
            if roles_result:
                return -roles_result
            count_result = cmp(a_nomination['patch_count'], b_nomination['patch_count'])
            if count_result:
                return -count_result
            return cmp(a_nomination['author_name'], b_nomination['author_name'])

        for nomination in sorted(nominations, nomination_cmp):
            # This is a little bit of a hack, but its convienent to just pass the nomination dictionary to the formating operator.
            nomination['roles_string'] = join_with_separators(nomination['roles']).upper()
            print "%(roles_string)s: %(author_name)s (%(author_email)s) has %(patch_count)s reviewed patches" % nomination
            counter = self._counters_by_email[nomination['author_email']]

            if self.show_commits:
                print counter['commits']

    def _print_counts(self, counters_by_email):
        def counter_cmp(a_tuple, b_tuple):
            # split the tuples
            # the second element is the "counter" structure
            _, a_counter = a_tuple
            _, b_counter = b_tuple

            count_result = cmp(a_counter['count'], b_counter['count'])
            if count_result:
                return -count_result
            return cmp(a_counter['latest_name'].lower(), b_counter['latest_name'].lower())

        for author_email, counter in sorted(counters_by_email.items(), counter_cmp):
            if author_email != counter['latest_email']:
                continue
            contributor = self._committer_list.contributor_by_email(author_email)
            author_name = counter['latest_name']
            patch_count = counter['count']
            counter['names'] = counter['names'] - set([author_name])
            counter['emails'] = counter['emails'] - set([author_email])

            alias_list = []
            for alias in counter['names']:
                alias_list.append(alias)
            for alias in counter['emails']:
                alias_list.append(alias)
            if alias_list:
                print "CONTRIBUTOR: %s (%s) has %d reviewed patches %s" % (author_name, author_email, patch_count, "(aliases: " + ", ".join(alias_list) + ")")
            else:
                print "CONTRIBUTOR: %s (%s) has %d reviewed patches" % (author_name, author_email, patch_count)
        return

    def execute(self, options, args, tool):
        self._init_options(options)
        patch_counts = self._count_recent_patches()
        nominations = self._collect_nominations(patch_counts)
        self._print_nominations(nominations)
        if self.verbose:
            self._print_counts(patch_counts)


if __name__ == "__main__":
    SuggestNominations()
