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

import sys

from .command import Command

from webkitbugspy import Tracker, bugzilla, radar
from webkitscmpy import log


class CreateBug(Command):
    name = 'create-bug'
    aliases = []
    help = 'Create a new bug on the configured bug tracker'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--title', dest='title', type=str, required=True,
            help='Bug title/summary',
        )
        parser.add_argument(
            '-F', '--file', dest='description_file', type=str, default=None,
            help='Path to file containing bug description',
        )
        parser.add_argument(
            '--component', dest='component', type=str, default=None,
            help='WebKit component (prompts if not specified)',
        )
        parser.add_argument(
            '--project', dest='project', type=str, default=None,
            help='Product name (prompts if not specified)',
        )
        parser.add_argument(
            '--keywords', dest='keywords', type=str, default=None,
            help='Comma-separated list of keywords',
        )
        parser.add_argument(
            '--cc', dest='cc', type=str, default=None,
            help='Comma-separated list of email addresses to CC',
        )
        parser.add_argument(
            '--assignee', dest='assignee', type=str, default=None,
            help='Username to assign bug to (defaults to current user)',
        )
        parser.add_argument(
            '--depends-on', dest='depends_on', type=int, action='append', default=None,
            help='Bug ID that this bug depends on (can be specified multiple times)',
        )
        parser.add_argument(
            '--blocks', dest='blocks', type=int, action='append', default=None,
            help='Bug ID that this bug blocks (can be specified multiple times)',
        )
        parser.add_argument(
            '--see-also', dest='see_also', action='append', default=None,
            help='URL to add to See Also (can be specified multiple times)',
        )
        parser.add_argument(
            '--radar', dest='radar', type=str, default=None,
            help='Radar ID to associate when creating this bug (e.g., rdar://12345678).',
        )

    @classmethod
    def parse_radar_arg(cls, value):
        """Validate and normalize a --radar argument to a single rdar:// string.

        Accepts a bare integer ID, rdar://NNNNN, rdar://problem/NNNNN,
        or https://rdar.apple.com/NNNNN.
        Raises ValueError for multiple IDs or unrecognized formats.
        Returns the normalized 'rdar://NNNNN' string.
        """
        value = value.strip()

        # Strip optional surrounding angle brackets (e.g., <rdar://N>)
        if value.startswith('<') and value.endswith('>'):
            value = value[1:-1].strip()

        # Bare integer ID
        if value.isdigit():
            return 'rdar://{}'.format(value)

        # Space or comma — multiple IDs
        if ' ' in value or ',' in value:
            raise ValueError('only one radar ID is accepted')

        # Delegate URL parsing to radar.Tracker.parse_id()
        result = radar.Tracker.parse_id(value)
        if result is None:
            raise ValueError("unrecognized format; expected a numeric ID or rdar://NNNNN")
        if isinstance(result, list):
            raise ValueError('only one radar ID is accepted')
        return 'rdar://{}'.format(result)

    @classmethod
    def main(cls, args, repository, **kwargs):
        tracker = Tracker.instance()
        if not tracker:
            sys.stderr.write("No bug tracker configured\n")
            return 1

        # Ensure credentials are available
        if getattr(tracker, 'credentials', None):
            tracker.credentials(required=True, validate=True)

        # Read description from file
        description = None
        if args.description_file:
            try:
                with open(args.description_file, 'r') as f:
                    description = f.read()
            except IOError as e:
                sys.stderr.write("Failed to read description file '{}': {}\n".format(
                    args.description_file, e))
                return 1

        if not description:
            sys.stderr.write("Bug description is required (use -F/--file)\n")
            return 1

        # Start with user-specified project/component
        project = args.project
        component = args.component

        # Check for radar and apply security override if redacted
        radar_issue = None
        security_forced = False
        if args.radar:
            try:
                radar_str = cls.parse_radar_arg(args.radar)
            except ValueError as e:
                sys.stderr.write("Invalid --radar value '{}': {}\n".format(args.radar, e))
                return 1
            radar_issue = Tracker.from_string(radar_str)
            if radar_issue:
                project, component, security_forced = bugzilla.Tracker.classify_from_radar(
                    radar_issue, project, component,
                )

        # If not forced to Security, check for security keywords in title + description
        if not security_forced:
            matches = bugzilla.Tracker.check_security_keywords(args.title, description)
            if matches:
                project, component = bugzilla.Tracker.prompt_security_classification(
                    matches, project, component,
                )

        # Parse keywords
        keywords = None
        if args.keywords:
            keywords = [k.strip() for k in args.keywords.split(',')]

        # Create the bug (tracker.create prompts for project/component if not specified)
        try:
            issue = tracker.create(
                title=args.title,
                description=description,
                project=project,
                component=component,
                keywords=keywords,
            )
        except Exception as e:
            sys.stderr.write("Failed to create bug: {}\n".format(e))
            return 1

        if not issue:
            sys.stderr.write("Failed to create bug\n")
            return 1

        print("Created bug: {}".format(issue.link))

        # Post-creation operations
        cls._post_create_operations(issue, tracker, args, radar_issue=radar_issue)

        return 0

    @classmethod
    def _post_create_operations(cls, issue, tracker, args, radar_issue=None):
        """Handle CC, assignee, see-also, depends-on, blocks, and radar linking after bug creation."""

        # Handle assignee
        if args.assignee:
            try:
                assignee_user = tracker.user(username=args.assignee)
                if assignee_user:
                    tracker.set(issue, assignee=assignee_user)
                    print("Assigned to: {}".format(args.assignee))
            except Exception as e:
                log.warning("Failed to assign: {}".format(e))

        # Handle CC list
        if args.cc:
            cc_list = [e.strip() for e in args.cc.split(',')]
            # Remove the radar importer if present — cc_radar() will handle it
            # to ensure the <rdar://N> comment and InRadar keyword come first.
            if isinstance(tracker, bugzilla.Tracker) and getattr(tracker, 'radar_importer', None):
                importer = tracker.radar_importer
                importer_ids = set(filter(None, [importer.username, importer.email] + list(importer.emails)))
                cc_list = [cc for cc in cc_list if cc not in importer_ids]
            if cc_list:
                try:
                    tracker.set(issue, cc=cc_list)
                    print("Added CC: {}".format(', '.join(cc_list)))
                except Exception as e:
                    log.warning("Failed to add CC: {}".format(e))

        # Handle See Also
        if args.see_also:
            try:
                tracker.set(issue, see_also=args.see_also)
                print("Added See Also: {}".format(', '.join(args.see_also)))
            except Exception as e:
                log.warning("Failed to add See Also: {}".format(e))

        # Handle relationships
        for attr, label, apply in [
            ('depends_on', 'depends on', lambda bug_id: tracker.relate(issue, depends_on=tracker.issue(bug_id))),
            ('blocks', 'blocks', lambda bug_id: tracker.relate(issue, blocks=tracker.issue(bug_id))),
        ]:
            for bug_id in getattr(args, attr) or []:
                try:
                    apply(bug_id)
                    print("Set {}: Bug {}".format(label, bug_id))
                except Exception as e:
                    log.warning("Failed to set {} {}: {}".format(attr, bug_id, e))

        # Link to radar: post <rdar://N> comment, add InRadar keyword, CC importer.
        # cc_radar handles the safe ordering to avoid duplicate radar creation.
        if isinstance(tracker, bugzilla.Tracker):
            try:
                tracker.cc_radar(issue, radar=radar_issue)
            except Exception as e:
                log.warning("Failed to cc radar importer: {}".format(e))
