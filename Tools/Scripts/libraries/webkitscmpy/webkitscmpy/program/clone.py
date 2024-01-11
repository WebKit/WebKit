# Copyright (C) 2023 Apple Inc. All rights reserved.
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
from .trace import Trace, Relationship
from webkitbugspy import Tracker, radar
from webkitcorepy import arguments, Terminal
from webkitscmpy import log


class Clone(Command):
    name = 'clone'
    help = 'Clone the radar a bugzilla or commit refers to'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit to be cherry-picked',
        )
        parser.add_argument(
            '--milestone', '-m',
            dest='milestone', default=None,
            help='Milestone to move cloned radar into',
            type=str,
        )
        parser.add_argument(
            '--why', '-w', '--reason', '-r',
            dest='reason', default=None,
            help='Reason why issue is being cloned',
            type=str,
        )
        parser.add_argument(
            '--prompt', '--no-prompt',
            dest='prompt', default=Terminal.isatty(sys.stdout),
            help='Enable (or disable) prompts to populate cloned radar',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--dry-run', '-d',
            dest='dry_run', default=False,
            help='Gather milestone, category, tentpole and event for clone, but do not preform clone',
            action='store_true',
        )
        parser.add_argument(
            '--merge-back',
            dest='merge_back', default=False,
            help='Annotate the cloned issue for merge-back',
            action='store_true',
        )

    @classmethod
    def main(cls, args, repository, merge_back=False, **kwargs):
        rdar = None
        merge_back = merge_back or args.merge_back
        prefix = '[merge-back]' if merge_back else ''

        for tracker in Tracker._trackers:
            if isinstance(tracker, radar.Tracker):
                rdar = tracker
                break
        if not rdar or not rdar.radarclient():
            sys.stderr.write("'cloning' is a concept which belongs solely to radar\n")
            if not rdar:
                sys.stderr.write('This repository does not declare radar as an issue tracker\n')
            else:
                sys.stderr.write('Radar is not available on this machine\n')
            return 255

        if not args.reason and args.milestone:
            args.reason = "Cloning for inclusion in '{}'".format(args.milestone)
        if not args.reason:
            sys.stderr.write('No reason for cloning issue has been provided\n')
            return 255

        # First, check if we were provided a bug URL
        issue = Tracker.from_string(args.argument[0])
        if not issue:
            if not repository:
                sys.stderr.write("Failed to resolve '{}' to a radar\n".format(args.argument[0]))
                sys.stderr.write('No repository to search for commits in\n')
                return 255
            commit = repository.find(args.argument[0], include_identifier=False)
            if not commit:
                sys.stderr.write("Failed to resolve '{}' to a radar\n".format(args.argument[0]))
                sys.stderr.write("No commit found matching '{}'\n".format(args.argument[0]))
                return 255
            for relation in (Trace.relationships(commit, repository) or []):
                if relation.type in Relationship.IDENTITY:
                    commit = relation.commit
                    break
            if not commit.issues:
                sys.stderr.write("Failed to resolve '{}' to a radar\n".format(args.argument[0]))
                sys.stderr.write("Found {}, but it doesn't reference any issues\n".format(commit))
                return 255
            for candidate in commit.issues:
                if isinstance(candidate.tracker, radar.Tracker):
                    issue = candidate
                    break
            if not issue:
                issue = commit.issues[0]
            log.info('{} references {}'.format(commit, issue))

        if not isinstance(issue.tracker, radar.Tracker):
            for candidate in issue.references:
                if isinstance(candidate.tracker, radar.Tracker):
                    issue = candidate
                    break
        if not isinstance(issue.tracker, radar.Tracker):
            sys.stderr.write("'{}' is not a radar, therefore cannot be cloned\n".format(issue))
            return 255

        milestone = None
        raw_issue = rdar.client.radar_for_id(issue.id)
        milestones = {
            milestone.name: milestone
            for milestone in rdar.client.milestones_for_component(raw_issue.component, include_access_groups=True)
        }
        if args.milestone:
            milestone = milestones.get(args.milestone, None)
        if args.milestone and not milestone:
            sys.stderr.write("Specified milestone '{}' not found in component '{}'\n".format(args.milestone, raw_issue.component))
        while True:
            if not milestone and not args.prompt:
                break
            if not milestone:
                milestone = milestones.get(Terminal.choose(
                    prompt='Pick a milestone for your clone',
                    options=list(milestones.keys()),
                    numbered=True,
                ), None)
                if not milestone:
                    continue
            if milestone.isClosed:
                sys.stderr.write("'{}' is closed, pick another\n".format(milestone.name))
                milestone = None
                continue
            if milestone.isRestricted:
                for group in milestone.restrictedAccessGroups:
                    name = group.name[4:] if group.name.startswith('DS: ') else group.name
                    if rdar.me().username in rdar.library.AppleDirectoryQuery.member_dsid_list_for_group_name(name):
                        break
                else:
                    sys.stderr.write("You do not have read access to '{}', pick another\n".format(milestone.name))
                    milestone = None
                    continue
            if milestone.isProtected:
                for group in milestone.protectedAccessGroups:
                    name = group.name[4:] if group.name.startswith('DS: ') else group.name
                    if rdar.me().username in rdar.library.AppleDirectoryQuery.member_dsid_list_for_group_name(name):
                        break
                else:
                    sys.stderr.write("You do not have write access to '{}', pick another\n".format(milestone.name))
                    milestone = None
                    continue
            break

        if not milestone:
            sys.stderr.write("Failed to find milestone matching '{}'\n".format(args.milestone))
            return 255

        if merge_back:
            milestone = milestones.get('Internal Tools - {}'.format(milestone.name), milestone)

        milestone_association = raw_issue.milestone_associations(milestone)

        def pick_attr(name, plural, default=None):
            mapping = {candidate.name: candidate for candidate in getattr(milestone_association, plural, [])}
            attr = getattr(raw_issue, name, None)
            if default or attr:
                value = mapping.get(default or attr.name, None)
                if value:
                    return value
                sys.stderr.write("{} '{}' does not exist in '{}'\n".format(name.capitalize(), default or attr.name, milestone.name))
            if not getattr(milestone_association, 'is{}Required'.format(name.capitalize()), None):
                return None
            value = mapping.get(Terminal.choose(
                prompt='Pick a {} for your clone'.format(name),
                options=sorted(mapping.keys()),
                numbered=True,
            ), None) if args.prompt else None
            if not value:
                sys.stderr.write("{} required for '{}', but not provided\n".format(name.capitalize(), milestone.name))
                return False
            return value

        category = pick_attr('category', 'categories', None)
        event = pick_attr('event', 'events')
        tentpole = pick_attr('tentpole', 'tentpoles')

        if False in (category, event, tentpole):
            sys.stderr.write('Too few attributes specified to clone {}\n'.format(issue))
            return 255

        if args.dry_run:
            print("Cloning into '{}'".format(milestone.name))
            print("    Reason: {}".format(args.reason))
            if prefix:
                print("    with tile '{} {}'".format(prefix, issue.title))
            if category:
                print("    with category '{}'".format(category.name))
            if event:
                print("    with event '{}'".format(event.name))
            if tentpole:
                print("    with tentpole '{}'".format(tentpole.name))
            return 255

        cloned = rdar.clone(issue, reason=args.reason)
        if not cloned:
            sys.stderr.write("Failed to clone '{}'\n".format(issue))
            return 255
        print("Created '{}{}'".format('{} '.format(prefix) if merge_back else '', cloned))

        raw_clone = rdar.client.radar_for_id(cloned.id)

        try:
            if prefix:
                raw_clone.title = '{} {}'.format(prefix, raw_issue.title)
            raw_clone.priority = raw_issue.priority
            raw_clone.resolution = raw_issue.resolution
            raw_clone.commit_changes()
        except rdar.radarclient().exceptions.UnsuccessfulResponseException:
            sys.stderr.write('Completed clone, but failed to set priority and resolution\n')
            return 1

        try:
            raw_clone.milestone = milestone
            raw_clone.category = category
            raw_clone.event = event
            raw_clone.tentpole = tentpole
            raw_clone.commit_changes()
        except rdar.radarclient().exceptions.UnsuccessfulResponseException:
            sys.stderr.write('Completed clone, but failed to set milestone, category, event and tentpole\n')
            return 1

        try:
            raw_clone.state = 'Analyze'
            if raw_clone.resolution:
                raw_clone.substate = 'Prepare'
            else:
                raw_clone.substate = 'Investigate'
                sys.stderr.write('{} does not have a resolution\n'.format(issue.link))
                sys.stderr.write('Placing {} in {}\n'.format(cloned.link, raw_clone.substate))
            raw_clone.commit_changes()
        except rdar.radarclient().exceptions.UnsuccessfulResponseException:
            sys.stderr.write("Completed clone and set milestone, but failed to move to 'Integrate'\n")
            return 1

        print('Moved clone to {} and into {}{}'.format(
            raw_clone.milestone.name,
            raw_clone.state,
            ': {}'.format(raw_clone.substate) if raw_clone.state == 'Analyze' else '',
        ))
        return 0
