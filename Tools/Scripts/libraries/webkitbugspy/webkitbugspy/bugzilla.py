# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

import calendar
import re
import requests
import sys
import time
import webkitcorepy

from .issue import Issue
from .tracker import Tracker as GenericTracker
from .radar import Tracker as RadarTracker

from datetime import datetime
from webkitbugspy import User, log


class Tracker(GenericTracker):
    ROOT_RE = re.compile(r'\Ahttps?://(?P<domain>\S+)\Z')
    RE_TEMPLATES = [
        r'\Ahttps?://{}/show_bug.cgi\?id=(?P<id>\d+)\Z',
        r'\A{}/show_bug.cgi\?id=(?P<id>\d+)\Z',
    ]
    NAME = 'Bugzilla'

    class Encoder(GenericTracker.Encoder):
        @webkitcorepy.decorators.hybridmethod
        def default(context, obj):
            if isinstance(obj, Tracker):
                result = dict(
                    type='bugzilla',
                    url=obj.url,
                )
                if obj._res[len(Tracker.RE_TEMPLATES):]:
                    result['res'] = [compiled.pattern for compiled in obj._res[len(Tracker.RE_TEMPLATES):]]
                if obj.radar_importer:
                    result['radar_importer'] = User.Encoder().default(obj.radar_importer)
                return result
            if isinstance(context, type):
                raise TypeError('Cannot invoke parent class when classmethod')
            return super(Tracker.Encoder, context).default(obj)

    def __init__(self, url, users=None, res=None, login_attempts=3, redact=None, radar_importer=None):
        super(Tracker, self).__init__(users=users, redact=redact)

        self._logins_left = login_attempts + 1 if login_attempts else 1
        match = self.ROOT_RE.match(url)
        if not match:
            raise TypeError("'{}' is not a valid bugzilla url".format(url))
        self.url = url
        self._res = [
            re.compile(template.format(match.group('domain')))
            for template in self.RE_TEMPLATES
        ] + (res or [])

        if radar_importer:
            self.radar_importer = User(**radar_importer) if isinstance(radar_importer, dict) else radar_importer
        else:
            self.radar_importer = None

    def user(self, name=None, username=None, email=None):
        user = super(Tracker, self).user(name=name, username=username, email=email)
        if user:
            return user
        if name and username and email:
            return self.users.create(
                name=name,
                username=username,
                emails=[email],
            )

        if not username and not email:
            raise RuntimeError("Failed to find username for '{}'".format(name))
        response = requests.get('{url}/rest/user{query}'.format(
            url=self.url,
            query=self._login_arguments(
                required=False,
                query='names={name}'.format(name=username or email or name),
            ),
        ))
        if response.status_code // 100 == 4 and self._logins_left:
            self._logins_left -= 1
        response = response.json().get('users') if response.status_code // 100 == 2 else None
        if not response:
            return self.users.create(
                name=name,
                username=username,
                emails=[email] if email else None,
            )

        return self.users.create(
            name=response[0]['real_name'],
            username=response[0]['name'],
            emails=[response[0]['name']],
        )

    def from_string(self, string):
        for regex in self._res:
            match = regex.match(string)
            if match:
                return self.issue(int(match.group('id')))
        return None

    def credentials(self, required=True, validate=False):
        def validater(username, password):
            quoted_username = requests.utils.quote(username)
            response = requests.get('{}/rest/user/{}?login={}&password={}'.format(self.url, quoted_username, quoted_username, requests.utils.quote(password)))
            if response.status_code == 200:
                return True
            sys.stderr.write('Login to {} for {} failed\n'.format(self.url, username))
            return False

        return webkitcorepy.credentials(
            url=self.url,
            required=required,
            prompt=self.url.split('//')[-1],
            validater=validater,
            validate_existing_credentials=validate
        )

    def _login_arguments(self, required=False, query=None):
        if not self._logins_left:
            if required:
                raise RuntimeError('Exhausted login attempts')
            return '?{}'.format(query) if query else ''

        username, password = self.credentials(required=required)
        if not username or not password:
            return '?{}'.format(query) if query else ''
        return '?login={username}&password={password}{query}'.format(
            username=requests.utils.quote(username),
            password=requests.utils.quote(password),
            query='&{}'.format(query) if query else '',
        )

    @webkitcorepy.decorators.Memoize()
    def me(self):
        username, _ = self.credentials(required=True)
        return self.user(username=username)

    def issue(self, id):
        return Issue(id=int(id), tracker=self)

    def populate(self, issue, member=None):
        issue._link = '{}/show_bug.cgi?id={}'.format(self.url, issue.id)
        issue._labels = []

        if member in ('title', 'timestamp', 'creator', 'opened', 'assignee', 'watchers', 'project', 'component', 'version'):
            response = requests.get('{}/rest/bug/{}{}'.format(self.url, issue.id, self._login_arguments(required=False)))
            if response.status_code // 100 == 4 and self._logins_left:
                self._logins_left -= 1
            response = response.json().get('bugs', []) if response.status_code == 200 else None
            if response:
                response = response[0]
                issue._title = response['summary']
                issue._timestamp = int(calendar.timegm(datetime.strptime(response['creation_time'], '%Y-%m-%dT%H:%M:%SZ').timetuple()))
                if response.get('creator_detail'):
                    issue._creator = self.user(
                        name=response['creator_detail'].get('real_name'),
                        username=response['creator_detail'].get('name'),
                        email=response['creator_detail'].get('email'),
                    )
                else:
                    issue._creator = self.user(username=response['creator']) if response.get('creator') else None
                issue._opened = response['status'] != 'RESOLVED'
                if response.get('assigned_to_detail'):
                    issue._assignee = self.user(
                        name=response['assigned_to_detail'].get('real_name'),
                        username=response['assigned_to_detail'].get('name'),
                        email=response['assigned_to_detail'].get('email'),
                    )
                else:
                    issue._assignee = self.user(username=response['assigned_to']) if response.get('assigned_to') else None
                issue._watchers = []
                for name in response.get('cc', []):
                    issue._watchers.append(self.user(username=name))

                issue._project = response.get('product', '')
                issue._component = response.get('component', '')
                issue._version = response.get('version', '')

            else:
                sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))

        if member in ['description', 'comments']:
            response = requests.get('{}/rest/bug/{}/comment{}'.format(self.url, issue.id, self._login_arguments(required=False)))
            if response.status_code // 100 == 4 and self._logins_left:
                self._logins_left -= 1
            if response.status_code == 200:
                response = response.json().get('bugs', {}).get(str(issue.id), {}).get('comments', None)
            else:
                response = None

            if response:
                issue._description = response[0].get('text')
                issue._comments = [
                    Issue.Comment(
                        user=self.user(username=comment['creator']),
                        timestamp=int(calendar.timegm(datetime.strptime(comment['creation_time'], '%Y-%m-%dT%H:%M:%SZ').timetuple())),
                        content=comment['text'],
                    ) for comment in response[1:]
                ]
            else:
                sys.stderr.write("Failed to fetch comments for '{}'\n".format(issue.link))

        if member == 'references':
            issue._references = []
            refs = set()

            # Attempt to match radar importer first
            if self.radar_importer:
                for comment in issue.comments:
                    if comment.user != self.radar_importer:
                        continue
                    candidate = GenericTracker.from_string(comment.content)
                    if not candidate or candidate.link in refs or (isinstance(type(candidate.tracker), type(issue.tracker)) and candidate.id == issue.id):
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)

            for text in [issue.description] + [comment.content for comment in issue.comments]:
                for match in self.REFERENCE_RE.findall(text):
                    candidate = GenericTracker.from_string(match[0]) or self.from_string(match[0])
                    if not candidate or candidate.link in refs or (isinstance(candidate.tracker, type(issue.tracker)) and candidate.id == issue.id):
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)

            response = requests.get('{url}/rest/bug/{id}{query}'.format(
                url=self.url, id=issue.id,
                query=self._login_arguments(required=False, query='include_fields=see_also'),
            ))
            if response.status_code // 100 == 4 and self._logins_left:
                self._logins_left -= 1
            response = response.json().get('bugs', []) if response.status_code == 200 else None
            if response:
                for link in response[0].get('see_also', []):
                    candidate = GenericTracker.from_string(link) or self.from_string(link)
                    if not candidate or candidate.link in refs or candidate.id == issue.id:
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)
            else:
                sys.stderr.write("Failed to fetch related issues for '{}'\n".format(issue.link))

        return issue

    def set(self, issue, assignee=None, opened=None, why=None, project=None, component=None, version=None, **properties):
        update_dict = dict()

        if properties:
            raise TypeError("'{}' is an invalid property".format(list(properties.keys())[0]))

        if assignee:
            if not isinstance(assignee, User):
                raise TypeError("Must assign to '{}', not '{}'".format(User, type(assignee)))
            issue._assignee = self.user(name=assignee.name, username=assignee.username, email=assignee.email)
            update_dict['assigned_to'] = issue._assignee.username

        if opened is not None:
            issue._opened = bool(opened)
            if issue._opened:
                update_dict['status'] = 'REOPENED'
                why = why or 'Reopening bug'
            else:
                update_dict['status'] = 'RESOLVED'
                update_dict['resolution'] = 'FIXED'

        if why is not None:
            update_dict['comment'] = dict(body=why)

        if project or component or version:
            if not project and len(self.projects) == 1:
                project = list(self.projects.keys())[0]
            if not project:
                raise ValueError('No project provided')
            if not self.projects.get(project):
                raise ValueError("'{}' is not a recognized project".format(project))

            components = sorted(self.projects.get(project, {}).get('components', {}).keys())
            if not component and len(components) == 1:
                component = components[0]
            if not component:
                raise ValueError('No component provided')
            if component and component not in components:
                raise ValueError("'{}' is not a recognized component of '{}'".format(component, project))

            versions = []
            if component:
                versions = self.projects.get(project, {}).get('components', {}).get(component, {}).get('versions', [])
            if not versions:
                versions = self.projects.get(project, {}).get('versions', [])
            if not version:
                version = versions[0]
            if version not in versions:
                raise ValueError("'{}' is not a recognized version in '{} {}'".format(version, project, component))

            update_dict['product'] = project
            update_dict['component'] = component
            update_dict['version'] = version

        if update_dict:
            update_dict['ids'] = [issue.id]
            response = None
            try:
                response = requests.put(
                    '{}/rest/bug/{}{}'.format(self.url, issue.id, self._login_arguments(required=True)),
                    json=update_dict,
                )
            except RuntimeError as e:
                sys.stderr.write('{}\n'.format(e))
            if response and response.status_code // 100 == 4 and self._logins_left:
                self._logins_left -= 1
            if not response or response.status_code // 100 != 2:
                if assignee:
                    issue._assignee = None
                if opened is not None:
                    issue._opened = None
                sys.stderr.write("Failed to modify '{}'\n".format(issue))
                return None
            elif project and component and version:
                issue._project = project
                issue._component = component
                issue._version = version

        return issue

    def add_comment(self, issue, text):
        response = None
        try:
            response = requests.post(
                '{}/rest/bug/{}/comment{}'.format(self.url, issue.id, self._login_arguments(required=True)),
                json=dict(comment=text),
            )
        except RuntimeError as e:
            sys.stderr.write('{}\n'.format(e))

        if response and response.status_code // 100 == 4 and self._logins_left:
            self._logins_left -= 1
        if not response or response.status_code // 100 != 2:
            sys.stderr.write("Failed to add comment to '{}'\n".format(issue))
            return None

        result = Issue.Comment(
            user=self.me(),
            timestamp=int(time.time()),
            content=text,
        )
        if not issue._comments:
            self.populate(issue, 'comments')
        issue._comments.append(result)

        return result

    @property
    @webkitcorepy.decorators.Memoize()
    def projects(self):
        response = requests.get('{}/rest/product_enterable{}'.format(self.url, self._login_arguments(required=False)))
        if response.status_code // 100 == 4 and self._logins_left:
            self._logins_left -= 1
        if response.status_code // 100 != 2:
            sys.stderr.write("Failed to retrieve project list'\n")
            return dict()

        result = dict()
        for id in response.json().get('ids', []):
            id_response = requests.get('{}/rest/product/{}{}'.format(self.url, id, self._login_arguments(required=False)))
            if response.status_code // 100 == 4 and self._logins_left:
                self._logins_left -= 1
            if response.status_code // 100 != 2:
                sys.stderr.write("Failed to query bugzilla about prod '{}'\n".format(id))
                continue
            for product in id_response.json()['products']:
                if not product['is_active']:
                    continue
                result[product['name']] = dict(
                    description=product['description'],
                    versions=[version['name'] for version in product['versions']],
                    components=dict(),
                )
                for component in product['components']:
                    if not component['is_active']:
                        continue
                    result[product['name']]['components'][component['name']] = dict(description=component['description'])

        return result

    def create(
        self, title, description,
        project=None, component=None, version=None, assign=True,
    ):
        if not title:
            raise ValueError('Must define title to create bug')
        if not description:
            raise ValueError('Must define description to create bug')

        if not project and len(self.projects.keys()) == 1:
            project = list(self.projects.keys())[0]
        elif not project:
            project = webkitcorepy.Terminal.choose(
                'What project should the bug be associated with?',
                options=sorted(self.projects.keys()), numbered=True,
            )
        if project not in self.projects:
            raise ValueError("'{}' is not a recognized product on {}".format(project, self.url))

        if not component and len(self.projects[project]['components'].keys()) == 1:
            component = list(self.projects[project]['components'].keys())[0]
        elif not component:
            component = webkitcorepy.Terminal.choose(
                "What component in '{}' should the bug be associated with?".format(project),
                options=sorted(self.projects[project]['components'].keys()), numbered=True,
            )
        if component not in self.projects[project]['components']:
            raise ValueError("'{}' is not a recognized component in '{}'".format(component, project))

        if not version:
            # This is the default option, aligned to webkit-patch behavior.
            # FIXME: We should make this class project agnostic by specifying this in trackers.json.
            version = "WebKit Nightly Build"
            if version not in self.projects[project]['versions']:
                # If the default option does not exist on the list, we pick the last one from versions.
                version = self.projects[project]['versions'][-1]
        if version not in self.projects[project]['versions']:
            raise ValueError("'{}' is not a recognized version for '{}'".format(version, project))

        params = dict(
            summary=title,
            description=description,
            product=project,
            component=component,
            version=version,
        )
        if assign:
            params['assigned_to'] = self.me().username

        response = None
        try:
            response = requests.post('{}/rest/bug{}'.format(self.url, self._login_arguments(required=True)), json=params)
        except RuntimeError as e:
            sys.stderr.write('{}\n'.format(e))
        if response and response.status_code // 100 == 4 and self._logins_left:
            self._logins_left -= 1
        if not response or response.status_code // 100 != 2:
            sys.stderr.write("Failed to create bug: {}\n".format(
                response.json().get('message', '?') if response else 'Login attempts exhausted'),
            )
            return None
        return self.issue(response.json()['id'])

    def cc_radar(self, issue, block=False, timeout=None, radar=None):
        if not self.radar_importer:
            sys.stderr.write('No radar importer specified\n')
            return None

        for tracker in Tracker._trackers:
            if isinstance(tracker, RadarTracker):
                break
        else:
            sys.stderr.write('Project does not define radar tracker\n')
            return None

        keyword_to_add = None
        comment_to_make = None
        user_to_cc = self.radar_importer.name if self.radar_importer not in issue.watchers else None
        if radar and isinstance(radar.tracker, RadarTracker):
            if radar not in issue.references:
                comment_to_make = '<rdar://problem/{}>'.format(radar.id)
            if user_to_cc:
                keyword_to_add = 'InRadar'
            elif comment_to_make:
                sys.stderr.write("{} already CCed '{}' and tracking a different bug\n".format(
                    self.radar_importer.name,
                    issue.references[0] if issue.references else '?',
                ))

        did_modify_cc = False
        if user_to_cc or keyword_to_add:
            log.info('CCing {}'.format(self.radar_importer.name))
            response = None
            try:
                data = dict(ids=[issue.id])
                if user_to_cc:
                    data['cc'] = dict(add=[self.radar_importer.username])
                if comment_to_make:
                    data['comment'] = dict(body=comment_to_make)
                if keyword_to_add:
                    data['keywords'] = dict(add=[keyword_to_add])
                response = requests.put(
                    '{}/rest/bug/{}{}'.format(self.url, issue.id, self._login_arguments(required=True)),
                    json=data,
                )
            except RuntimeError as e:
                sys.stderr.write('{}\n'.format(e))
            if response and response.status_code // 100 == 4 and self._logins_left:
                self._logins_left -= 1
            if not response or response.status_code // 100 != 2:
                sys.stderr.write("Failed to cc {} on '{}'\n".format(self.radar_importer.name, issue))
            elif radar and isinstance(radar.tracker, RadarTracker):
                if comment_to_make:
                    issue._references = None
                    issue._comments.append(Issue.Comment(
                        user=self.me(),
                        timestamp=int(time.time()),
                        content=comment_to_make,
                    ))
                return radar
            else:
                did_modify_cc = True
                issue._comments = None
                issue._references = None

        start = time.time()
        while start + (timeout or 60) > time.time():
            for reference in issue.references:
                if isinstance(reference.tracker, RadarTracker):
                    return reference
            if not block or not did_modify_cc:
                break
            log.warning('Waiting until {} imports bug...'.format(self.radar_importer.name))
            time.sleep(10)
            issue._comments = None
            issue._references = None

        return None
