# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

from webkitbugspy import Issue, User


USERS = User.Mapping()
USERS.create(
    name='Tim Contributor',
    emails=['tcontributor@example.com'],
    username='tcontributor@example.com',
)
USERS.create(
    name='Felix Filer',
    emails=['ffiler@example.com'],
    username='ffiler@example.com',
)
USERS.create(
    name='Wilma Watcher',
    emails=['wwatcher@example.com'],
    username='wwatcher@example.com',
)
USERS.create(
    name='Radar WebKit Bug Importer',
    emails=['webkit-bug-importer@group.apple.com'],
    username='webkit-bug-importer@group.apple.com',
)

ISSUES = [
    dict(
        title='Example issue 1',
        timestamp=1639536160,
        opened=True,
        creator=USERS['Felix Filer'],
        assignee=USERS['Tim Contributor'],
        description='An example issue for testing',
        project='WebKit',
        component='Text',
        version='Other',
        milestone='October',
        keywords=['Keyword A'],
        comments=[
            Issue.Comment(
                user=USERS['Felix Filer'],
                timestamp=1639536220,
                content='Was able to reproduce on version 1.2.3',
            ), Issue.Comment(
                user=USERS['Tim Contributor'],
                timestamp=1639550220,
                content='Trying to prepare a fix for 1.2.4',
            ),
        ], watchers=[USERS['Felix Filer'], USERS['Tim Contributor'], USERS['Wilma Watcher']],
    ), dict(
        title='Example feature 1',
        timestamp=1639536320,
        opened=True,
        creator=USERS['Tim Contributor'],
        assignee=USERS['Tim Contributor'],
        description='We need to support a new feature',
        project='WebKit',
        component='Scrolling',
        version='Safari 15',
        milestone='October',
        keywords=['Keyword A'],
        comments=[
            Issue.Comment(
                user=USERS['Tim Contributor'],
                timestamp=1639539110,
                content="Version 1.3.0 should behave differently, let's add a feature!",
            ),
        ], watchers=[USERS['Tim Contributor'], USERS['Wilma Watcher']],
        references=[3],
    ), dict(
        title='Example issue 2',
        timestamp=1639540010,
        opened=False,
        creator=USERS['Felix Filer'],
        assignee=USERS['Tim Contributor'],
        description='Another example issue for testing, example.com is broken',
        project='WebKit',
        component='SVG',
        version='WebKit Local Build',
        milestone='October',
        keywords=['Keyword B'],
        comments=[
            Issue.Comment(
                user=USERS['Tim Contributor'],
                timestamp=1639544065,
                content="This is really a feature request, I already filed a bug for this.",
            ),
        ], watchers=[USERS['Felix Filer'], USERS['Tim Contributor'], USERS['Wilma Watcher']], references=[2],
    ),
]

PROJECTS = dict(
    WebKit=dict(
        id=1,
        description='The WebKit browser engine',
        versions=['Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
        components=dict(
            Scrolling=dict(description='Bugs related to main thread and off-main thread scrolling'),
            SVG=dict(description='For bugs in the SVG implementation.'),
            Tables=dict(description='For bugs specific to tables (both the DOM and rendering issues).'),
            Text=dict(description='For bugs in text layout and rendering, including international text support.'),
        ),
    ), CFNetwork=dict(
        id=2,
        description='Darwin networking framework',
        versions=['All'],
        components=dict(
            IPv4=dict(description='Bugs involving IPv4 networking'),
            IPv6=dict(description='Bugs involving IPv6 networking'),
        ),
    ),
)
