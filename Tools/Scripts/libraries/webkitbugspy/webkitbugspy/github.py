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

from datetime import datetime
from requests.auth import HTTPBasicAuth
from webkitbugspy import User


class Tracker(GenericTracker):
    ROOT_RE = re.compile(r'\Ahttps?://github.(?P<domain>\S+)/(?P<owner>\S+)/(?P<repository>\S+)\Z')
    ISSUE_LINK_RE = re.compile(r'#(?P<id>\d+)')
    USERNAME_RE = re.compile(r'(^|\s|\'|")@(?P<username>[^\s"\'<>]+)')
    RE_TEMPLATES = [
        r'\Ahttps?://github.{}/{}/{}/issues/(?P<id>\d+)\Z',
        r'\Agithub.{}/{}/{}/issues/(?P<id>\d+)\Z',
        r'\Ahttps?://api.github.{}/repos/{}/{}/issues/(?P<id>\d+)\Z',
        r'\Aapi.github.{}/repos/{}/{}/issues/(?P<id>\d+)\Z',
    ]
    REFRESH_TOKEN_PROMPT = "Is your API token out of date? Run 'git-webkit setup' to refresh credentials\n"
    DEFAULT_COMPONENT_COLOR = 'FFFFFF'
    DEFAULT_VERSION_COLOR = 'EEEEEE'
    ACCEPT_HEADER = 'application/vnd.github.v3+json'
    ERROR_MAP = {
        'missing': 'A resource does not exist.',
        'missing_field': 'A required field on a resource has not been set.',
        'invalid': 'The formatting of a field is invalid. Review the documentation for more specific information.',
        'already_exists': 'Another resource has the same value as this field. This can happen in resources that must have some unique key (such as label names).',
        'unprocessable': 'The inputs provided were invalid.'
    }
    NAME = 'GitHub Issue'


    class Encoder(GenericTracker.Encoder):
        @webkitcorepy.decorators.hybridmethod
        def default(context, obj):
            if isinstance(obj, Tracker):
                result = dict(
                    type='github',
                    url=obj.url,
                )
                if obj._res[len(Tracker.RE_TEMPLATES):]:
                    result['res'] = [compiled.pattern for compiled in obj._res[len(Tracker.RE_TEMPLATES):]]
                return result
            if isinstance(context, type):
                raise TypeError('Cannot invoke parent class when classmethod')
            return super(Tracker.Encoder, context).default(obj)

    def __init__(
            self, url, users=None, res=None,
            component_color=DEFAULT_COMPONENT_COLOR,
            version_color=DEFAULT_VERSION_COLOR,
            session=None, redact=None,
    ):
        super(Tracker, self).__init__(users=users, redact=redact)

        self.session = session or requests.Session()
        self.component_color = component_color
        self.version_color = version_color

        match = self.ROOT_RE.match(url)
        if not match:
            raise TypeError("'{}' is not a valid GitHub project".format(url))

        self._res = [
            re.compile(template.format(match.group('domain'), match.group('owner'), match.group('repository')))
            for template in self.RE_TEMPLATES
        ] + (res or [])

        self.url = url
        self.api_url = 'https://api.github.{}'.format(match.group('domain'))
        self.owner = match.group('owner')
        self.name = match.group('repository')

    def from_string(self, string):
        for regex in self._res:
            match = regex.match(string)
            if match:
                return self.issue(int(match.group('id')))
        return None

    def credentials(self, required=True, validate=False, save_in_keyring=None):
        def validater(username, access_token):
            if '@' in username:
                sys.stderr.write("Provided username contains an '@' symbol. Please make sure to enter your GitHub username, not an email associated with the account\n")
                return False
            response = self.session.get(
                '{}/user'.format(self.api_url),
                headers=dict(Accept=self.ACCEPT_HEADER),
                auth=HTTPBasicAuth(username, access_token),
            )
            expiration = response.headers.get('github-authentication-token-expiration', None)
            if expiration:
                expiration = int(calendar.timegm(datetime.strptime(expiration, '%Y-%m-%d %H:%M:%S UTC').timetuple()))
            if (expiration is None or expiration > time.time()) and response.status_code == 200 and response.json().get('login') == username:
                return True
            sys.stderr.write('Login to {} for {} failed\n'.format(self.api_url, username))
            return False

        hostname = self.url.split('/')[2]
        token_url = 'https://{}/settings/tokens/new'.format(hostname)

        def prompt():
            result = "GitHub's API\nProvide {} username and access token to create and update pull requests".format(hostname)
            if webkitcorepy.Terminal.open_url(
                '{}?scopes=repo,workflow&description={}%20Local%20Automation'.format(token_url, self.name),
                prompt='Please press Return key to open the GitHub token generation web page.\n'
                        'Options are preconfigured, set your expiration date and then click "Generate token": ',
            ):
                return result
            return '''{}
Please go to {token_url} and generate a new 'Personal access token' via 'Developer settings'
with 'repo' and 'workflow' access and appropriate 'Expiration' for your {host} user'''.format(result, token_url=token_url, host=hostname)

        return webkitcorepy.credentials(
            url=self.api_url,
            required=required,
            name=self.url.split('/')[2].replace('.', '_').upper(),
            prompt=prompt,
            key_name='token',
            validater=validater,
            validate_existing_credentials=validate,
            save_in_keyring=save_in_keyring,
        )

    def request(self, path=None, params=None, method='GET', headers=None, authenticated=None, paginate=True, json=None, error_message=None):
        headers = {key: value for key, value in headers.items()} if headers else dict()
        headers['Accept'] = headers.get('Accept', self.ACCEPT_HEADER)

        username, access_token = self.credentials(required=bool(authenticated))
        auth = HTTPBasicAuth(username, access_token) if username and access_token else None
        if authenticated is False:
            auth = None
        if authenticated and not auth:
            raise RuntimeError('Request requires authentication, none provided')

        params = {key: value for key, value in params.items()} if params else dict()
        params['per_page'] = params.get('per_page', 100)
        params['page'] = params.get('page', 1)

        url = '{api_url}/repos/{owner}/{name}/{path}'.format(
            api_url=self.api_url,
            owner=self.owner,
            name=self.name,
            path='{}'.format(path) if path else '',
        )
        response = self.session.request(method, url, params=params, headers=headers, auth=auth, json=json)
        if authenticated is None and not auth and response.status_code // 100 == 4:
            return self.request(path=path, params=params, method=method, headers=headers, authenticated=True, paginate=paginate, json=json, error_message=error_message)
        if response.status_code // 100 != 2:
            if error_message:
                sys.stderr.write("{}\n".format(error_message))
            sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
            message = response.json().get('message')
            if message:
                sys.stderr.write('Message: {}\n'.format(message))
            if auth:
                sys.stderr.write(self.REFRESH_TOKEN_PROMPT)
            return None
        result = response.json()

        while paginate and isinstance(response.json(), list) and len(response.json()) == params['per_page']:
            params['page'] += 1
            response = self.session.get(url, params=params, headers=headers, auth=auth)
            if response.status_code != 200:
                raise OSError("Failed to assemble pagination requests for '{}', failed on page {}".format(url, params['page']))
            result += response.json()
        return result

    def user(self, name=None, username=None, email=None):
        user = super(Tracker, self).user(name=name, username=username, email=email)
        if user:
            return user
        if not username:
            raise RuntimeError("Failed to find username for '{}'".format(name or email))

        url = '{api_url}/users/{username}'.format(api_url=self.api_url, username=username)
        response = self.session.get(
            url, auth=HTTPBasicAuth(*self.credentials(required=True)),
            headers=dict(Accept=self.ACCEPT_HEADER),
        )
        if response.status_code // 100 != 2:
            sys.stderr.write("Request to '{}' returned status code '{}'\n".format(url, response.status_code))
            message = response.json().get('message')
            if message:
                sys.stderr.write('Message: {}\n'.format(message))
            sys.stderr.write(self.REFRESH_TOKEN_PROMPT)
            return None

        data = response.json()
        return self.users.create(
            name=data.get('name', username),
            username=username,
            emails=[data.get('email')],
        )

    @webkitcorepy.decorators.Memoize()
    def me(self):
        username, _ = self.credentials(required=True)
        return self.user(username=username)

    def issue(self, id):
        return Issue(id=int(id), tracker=self)

    def populate(self, issue, member=None):
        issue._link = '{}/issues/{}'.format(self.url, issue.id)
        issue._project = self.name

        if member in ('title', 'timestamp', 'creator', 'opened', 'assignee', 'description', 'project', 'component', 'version', 'labels'):
            response = self.request(path='issues/{}'.format(issue.id))
            if response:
                issue._title = response['title']
                issue._timestamp = int(calendar.timegm(datetime.strptime(response['created_at'], '%Y-%m-%dT%H:%M:%SZ').timetuple()))
                issue._creator = self.user(username=response['user']['login']) if response.get('user') else None
                issue._description = response['body']
                issue._opened = response['state'] != 'closed'
                issue._assignee = self.user(username=response['assignee']['login']) if response.get('assignee') else None

                issue._labels = []
                for label in response.get('labels', []):
                    if not label.get('name'):
                        continue
                    issue._labels.append(label['name'])
                    if self.component_color and label.get('color') == self.component_color:
                        issue._component = label['name']
                    if self.version_color and label.get('color') == self.version_color:
                        issue._version = label['name']
            else:
                sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))

        if member == 'watchers':
            issue._watchers = []
            refs = set()
            response = self.request(path='issues/{}'.format(issue.id))
            if response:
                for assignee in response['assignees']:
                    watcher = self.user(username=assignee['login'])
                    if not watcher or watcher.username in refs:
                        continue
                    refs.add(watcher.username)
                    issue._watchers.append(watcher)
            else:
                sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))

            for text in [issue.description] + [comment.content for comment in issue.comments]:
                for match in self.USERNAME_RE.findall(text):
                    if match[1] in refs:
                        continue
                    user = self.user(username=match[1])
                    if not user:
                        continue
                    refs.add(user.username)
                    issue._watchers.append(user)

        if member == 'comments':
            response = self.request(path='issues/{}/comments'.format(issue.id))
            issue._comments = []
            for node in response or []:
                username = node.get('user', {}).get('login')
                if not username:
                    continue
                tm = node.get('updated_at', node.get('created_at'))
                if tm:
                    tm = int(calendar.timegm(datetime.strptime(tm, '%Y-%m-%dT%H:%M:%SZ').timetuple()))

                issue._comments.append(Issue.Comment(
                    user=self.user(username=username),
                    timestamp=tm,
                    content=node.get('body'),
                ))

        if member == 'references':
            issue._references = []
            refs = set()

            for text in [issue.description] + [comment.content for comment in issue.comments]:
                for match in self.ISSUE_LINK_RE.findall(text):
                    candidate = self.issue(int(match))
                    if candidate.link in refs or candidate.id == issue.id:
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)

                for match in self.REFERENCE_RE.findall(text):
                    candidate = GenericTracker.from_string(match[0])
                    if not candidate or candidate.link in refs or candidate.id == issue.id:
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)

            response = self.request(path='issues/{}/timeline'.format(issue.id))
            if response:
                for event in response:
                    if not event.get('event'):
                        continue
                    id = event.get('source', {}).get('issue', {}).get('number', None)
                    if not id:
                        continue
                    candidate = self.issue(id)
                    if candidate.link in refs or candidate.id == issue.id:
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)

        return issue

    def set(self, issue, assignee=None, opened=None, why=None, project=None, component=None, version=None, labels=None, **properties):
        update_dict = dict()

        if properties:
            raise TypeError("'{}' is an invalid property".format(list(properties.keys())[0]))

        if assignee:
            if not isinstance(assignee, User):
                raise TypeError("Must assign to '{}', not '{}'".format(User, type(assignee)))
            issue._assignee = self.user(name=assignee.name, username=assignee.username, email=assignee.email)
            assignees = [issue._assignee.username]
            if self.add_assignees(issue, assignees) != assignees:
                return None

        if opened is not None:
            issue._opened = bool(opened)
            update_dict['state'] = 'open' if issue._opened else 'closed'

        if project or component or version:
            if not self.projects:
                raise ValueError("No components are defined for '{}'".format(self.url))
            project = project or self.name
            if project != self.name:
                raise ValueError("Project should be '{}' not '{}'".format(self.name, project))

            components = sorted(self.projects.get(project, {}).get('components', {}).keys())
            if not component and len(components) == 1:
                component = components[0]
            if component and component not in components:
                raise ValueError("'{}' is not a recognized component of '{}'".format(component, project))

            versions = self.projects.get(project, {}).get('versions', [])
            if not version and len(versions) == 1:
                version = versions[0]
            if version and version not in versions:
                raise ValueError("'{}' is not a recognized version of '{}'".format(version, project))

            labels = (labels or issue.labels)
            index = 0
            while index < len(labels):
                label = labels[index]
                color = self.labels.get(label, {}).get('color')
                if color and component and color == self.component_color:
                    labels.pop(index)
                elif color and version and color == self.version_color:
                    labels.pop(index)
                else:
                    index += 1
            if component:
                labels.append(component)
            if version:
                labels.append(version)

        if labels is not None:
            for label in labels:
                if not self.labels.get(label):
                    raise ValueError("'{}' is not a label for '{}'".format(label, self.url))
            response = self.request(
                'issues/{id}/labels'.format(id=issue.id),
                method='PUT',
                authenticated=True,
                json=dict(labels=labels),
                error_message="Failed to modify '{}'".format(issue)
            )
            if not response:
                if not update_dict:
                    return None
            elif project and component and version:
                issue._project = project
                issue._component = component
                issue._version = version

        if update_dict:
            update_dict['number'] = [issue.id]
            response = self.request(
                'issues/{id}'.format(id=issue.id),
                method='PATCH',
                authenticated=True,
                json=update_dict,
                error_message="Failed to modify '{}'".format(issue)
            )
            if not response:
                if opened is not None:
                    issue._opened = None
                return None

        return self.add_comment(issue, why) if why else issue

    def add_assignees(self, issue, assignees):
        response = self.request(
            'issues/{id}/assignees'.format(id=issue.id),
            method='POST',
            authenticated=True,
            json={'assignees': assignees},
            error_message="Failed to modify '{}'".format(issue)
        )
        if not response:
            issue._assignee = None
            sys.stderr.write('Could not add any assignee(s) to issue')
            return []
        response_assignees = [assignee.get('login') for assignee in response.get('assignees', [])]
        missed_assignees = [assignee for assignee in assignees if assignee not in response_assignees]
        if missed_assignees:
            sys.stderr.write('Could not assign {} to issue'.format(missed_assignees))
        return response_assignees

    def add_comment(self, issue, text):
        data = self.request(
            'issues/{id}/comments'.format(id=issue.id),
            method='POST',
            authenticated=True,
            json=dict(body=text),
            error_message="Failed to add comment to '{}'".format(issue)
        )
        if not data:
            return None

        tm = data.get('updated_at', data.get('created_at'))
        if tm:
            tm = int(calendar.timegm(datetime.strptime(tm, '%Y-%m-%dT%H:%M:%SZ').timetuple()))
        else:
            tm = time.time()

        result = Issue.Comment(
            user=self.user(username=data.get('user', {}).get('login')),
            timestamp=tm,
            content=data.get('body'),
        )
        if issue._comments is None:
            self.populate(issue, 'comments')
        else:
            issue._comments.append(result)
        return result

    @property
    @webkitcorepy.decorators.Memoize()
    def labels(self):
        response = self.request('labels')
        result = {}
        for label in response:
            if 'name' not in label:
                continue
            result[label['name']] = dict(
                color=label.get('color'),
                description=label.get('description'),
            )
        return result

    @property
    def projects(self):
        result = dict(
            versions=[],
            components=dict(),
        )
        for name, details in self.labels.items():
            if details.get('color') == self.component_color:
                result['components'][name] = details
            elif details.get('color') == self.version_color:
                result['versions'].append(name)

        result['versions'] = list(sorted(result['versions']))
        if result['versions'] or result['components']:
            return {self.name: result}
        return {}

    def create(
        self, title, description,
        project=None, component=None, version=None,
        labels=None, assign=True,
    ):
        if not title:
            raise ValueError('Must define title to create issue')
        if not description:
            raise ValueError('Must define description to create issue')

        if self.projects:
            if not project and len(self.projects.keys()) == 1:
                project = list(self.projects.keys())[0]
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

            if version:
                if version not in self.projects[project]['versions']:
                    raise ValueError("'{}' is not a recognized version for '{}'".format(version, project))

        else:
            if project:
                raise ValueError("No 'projects' defined, cannot specify 'project'")
            if component:
                raise ValueError("No 'projects' defined, cannot specify 'component'")
            if version:
                raise ValueError("No 'projects' defined, cannot specify 'version'")

        labels = labels or []
        if component:
            labels.append(component)
        if version:
            labels.append(version)

        for label in labels or []:
            if label not in self.labels:
                raise ValueError("'{}' is not a recognized label in '{}'".format(label, self.url))

        data = dict(
            title=title,
            body=description,
        )
        if labels:
            data['labels'] = labels
        if assign:
            data['assignee'] = self.me().username

        response = self.request(
            'issues',
            method='POST',
            authenticated=True,
            json=data,
            error_message="Failed to create issue"
        )
        if not response:
            return None

        return self.issue(response['number'])

    def parse_error(self, json):
        message = json.get('message', 'Empty Message')
        error_messages = []
        for error in json.get('errors', []):
            error_message = '---\tERROR\t---\n'

            code = error.get('code')
            if code:
                type = self.ERROR_MAP.get(code, '')
                if code == 'custom':
                    type = error['message']

                error_message += 'Type: {}\n'.format(type)

            resource = error.get('resource')
            if resource:
                error_message += 'Resource: {}\n'.format(resource)

            field = error.get('field')
            if field:
                error_message += 'Field: {}\n'.format(field)

            error_message += '---\t---\t---\n'
            error_messages.append(error_message)

        documentation_url = json.get('documentation_url')
        if documentation_url:
            error_messages += 'Documentation URL: {}\n'.format(documentation_url)

        return 'Error Message: {}\n{}'.format(message, ''.join(error_messages))

    def cc_radar(self, issue, block=False, timeout=None, radar=None):
        sys.stderr.write('No radar CC implemented for GitHub Issues\n')
        return None
