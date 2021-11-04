# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import time

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Terminal as MockTerminal, Time as MockTime
from webkitscmpy import Contributor, Commit, local, program, mocks


def repository(path, has_oops=True, remote=None, git_svn=False):
    branch = 'eng/example'
    result = mocks.local.Git(path, remote=remote, git_svn=git_svn)
    result.commits[branch] = [Commit(
        hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
        identifier='3.1@{}'.format(branch),
        timestamp=int(time.time()) - 60,
        author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
        message='To Be Committed\n\nReviewed by {}.\n'.format(
            'NOBODY (OOPS!)' if has_oops else 'Ricky Reviewer',
        ),
    )]
    result.head = result.commits[branch][0]
    return result


class TestLand(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestLand, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_non_editable(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('land',),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '5@main')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual([line for line in log if 'Mock process' not in line], [])
        self.assertEqual(
            captured.stderr.getvalue(),
            "Can only 'land' editable branches\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_with_oops(self):
        with OutputCapture() as captured, repository(self.path), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('land',),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n"
            "Found '(OOPS!)' message in commit messages, please resolve before committing\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_default(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('land',),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(captured.stdout.getvalue(), 'Landed a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd!\n')

    def test_canonicalize(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('land',),
                path=self.path,
                identifier_template='Canonical link: https://commits.webkit.org/{}',
            ))

            commit = local.Git(self.path).commit(branch='main')
            self.assertEqual(str(commit), '6@main')
            self.assertEqual(
                commit.message,
                'To Be Committed\n\n'
                'Reviewed by Ricky Reviewer.\n\n'
                'Canonical link: https://commits.webkit.org/6@main',
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                '1 commit to be editted...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd (1/1) (--- seconds passed, remaining --- predicted)\n'
            '1 commit successfully canonicalized!\n'
            'Landed https://commits.webkit.org/6@main (a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd)!\n',
        )

    def test_no_svn_canonical_svn(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('land',),
                path=self.path, canonical_svn=True,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        self.assertEqual(
            captured.stderr.getvalue(),
            "Cannot 'land' on a canonical SVN repository that is not configured as git-svn\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_svn(self):
        self.maxDiff = None
        with MockTime, OutputCapture() as captured, repository(self.path, has_oops=False, git_svn=True), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('land',),
                path=self.path, canonical_svn=True,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                '    Verifying mirror processesed change',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Landed a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd!\n',
        )


class TestLandGitHub(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestLandGitHub, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    @classmethod
    def webserver(cls, approved=None):
        result = mocks.remote.GitHub()
        result.users = dict(
            rreviewer=Contributor('Ricky Reviewer', ['rreviewer@webkit.org'], github='rreviewer'),
            tcontributor=Contributor('Tim Contributor', ['tcontributor@webkit.org'], github='tcontributor'),
        )
        result.issues = {
            1: dict(
                comments=[],
                assignees=[],
            )
        }
        result.pull_requests = [dict(
            number=1,
            state='open',
            title='Example Change',
            user=dict(login='tcontributor'),
            body='''#### a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd
    <pre>
    To Be Committed

    Reviewed by NOBODY (OOPS!).
    </pre>
    ''',
            head=dict(ref='username:eng/example'),
            base=dict(ref='main'),
            requested_reviews=[dict(login='rreviewer')],
            reviews=[
                dict(user=dict(login='rreviewer'), state='APPROVED')
            ] if approved else [] + [
                dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')
            ] if approved is not None else [], _links=dict(
                issue=dict(href='https://{}/issues/1'.format(result.api_remote)),
            ),
        )]
        return result

    def test_no_reviewer(self):
        with OutputCapture() as captured, self.webserver() as remote, \
                repository(self.path, remote='https://{}'.format(remote.remote)), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land',),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Found '(OOPS!)' message in commit messages, please resolve before committing\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_blocking_reviewer(self):
        with OutputCapture() as captured, self.webserver(approved=False) as remote, \
                repository(self.path, has_oops=False, remote='https://{}'.format(remote.remote)), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land',),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Ricky Reviewer is blocking landing 'PR 1 | Example Change'\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_insert_review(self):
        with OutputCapture() as captured, MockTerminal.input('y'), self.webserver(approved=True) as remote, \
                repository(self.path, has_oops=True, remote='https://{}'.format(remote.remote)), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('land',),
                path=self.path,
            ))

            repo = local.Git(self.path)
            self.assertEqual(str(repo.commit()), '6@main')
            self.assertEqual(
                [comment.content for comment in repo.remote().pull_requests.get(1).comments],
                ['Landed a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd!'],
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                'Setting Ricky Reviewer as reviewer',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
            ],
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Set 'Ricky Reviewer' as your reviewer? (Yes/No): \n"
            'Landed a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd!\n',
        )


class TestLandBitBucket(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestLandBitBucket, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    @classmethod
    def webserver(cls, approved=None):
        result = mocks.remote.BitBucket()
        result.pull_requests = [dict(
            id=1,
            state='OPEN',
            open=True,
            closed=False,
            activities=[],
            title='Example Change',
            author=dict(
                user=dict(
                    name='tcontributor',
                    emailAddress='tcontributor@apple.com',
                    displayName='Tim Contributor',
                ),
            ), body='''#### a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd
    ```
    To Be Committed

    Reviewed by NOBODY (OOPS!).
    ```
    ''',
            fromRef=dict(displayId='eng/example', id='refs/heads/eng/example'),
            toRef=dict(displayId='main', id='refs/heads/main'),
            reviewers=[
                dict(
                    user=dict(
                        displayName='Ricky Reviewer',
                        emailAddress='rreviewer@webkit.org',
                    ), approved=True if approved else False,
                    status='NEEDS_WORK' if approved is False else None,
                ),
            ],
        )]
        return result

    def test_no_reviewer(self):
        with OutputCapture() as captured, self.webserver() as remote, repository(
                self.path, remote='ssh://git@{}/{}/{}.git'.format(
                    remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
                )), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land',),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Found '(OOPS!)' message in commit messages, please resolve before committing\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_blocking_reviewer(self):
        with OutputCapture() as captured, self.webserver(approved=False) as remote, repository(
                self.path, has_oops=False, remote='ssh://git@{}/{}/{}.git'.format(
                    remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
                )), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land',),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Ricky Reviewer is blocking landing 'PR 1 | Example Change'\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_insert_review(self):
        with OutputCapture() as captured, MockTerminal.input('y'), self.webserver(approved=True) as remote, repository(
                self.path, has_oops=True, remote='ssh://git@{}/{}/{}.git'.format(
                    remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
                )), mocks.local.Svn():

            self.assertEqual(0, program.main(
                args=('land',),
                path=self.path,
            ))
            repo = local.Git(self.path)
            self.assertEqual(str(repo.commit()), '6@main')
            self.assertEqual(
                [comment.content for comment in repo.remote().pull_requests.get(1).comments],
                ['Landed a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd!'],
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                'Setting Ricky Reviewer as reviewer',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
            ],
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Set 'Ricky Reviewer' as your reviewer? (Yes/No): \n"
            'Landed a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd!\n',
        )
