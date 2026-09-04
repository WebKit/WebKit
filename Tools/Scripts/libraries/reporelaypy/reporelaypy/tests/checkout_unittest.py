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

import os
import subprocess
import unittest

from reporelaypy import Checkout
from webkitcorepy import testing, OutputCapture
from webkitscmpy import mocks


class CheckoutUnittest(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(CheckoutUnittest, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    def test_json(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(
                path=self.path,
                url=repo.remote,
                credentials={
                    'https://github.com': dict(
                        username='username',
                        password='password',
                    ),
                }, sentinal=False,
            )

            encoded = Checkout.Encoder().default(checkout)
            self.assertEqual(
                encoded, dict(
                    path=self.path,
                    url=repo.remote,
                    remotes={},
                    forwarding=[('origin', [])],
                    credentials={
                        'https://github.com': dict(
                            username='username',
                            password='password',
                        ),
                    }, sentinal=False,
                ),
            )

            constructed = Checkout(**encoded)
            self.assertEqual(checkout.path, constructed.path)
            self.assertEqual(checkout.url, constructed.url)
            self.assertEqual(checkout.remotes, constructed.remotes)
            self.assertEqual(checkout.credentials, constructed.credentials)


    def test_constructor_sentinal(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            with open(os.path.join(os.path.dirname(self.path), 'cloned'), 'w') as sentinal:
                sentinal.write('yes\n')
            Checkout(path=self.path, url=repo.remote, sentinal=True)

    def test_constructor_no_sentinal(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            Checkout(path=self.path, url=repo.remote, sentinal=False)

            with self.assertRaises(Checkout.Exception):
                Checkout(path=self.path, url=None, sentinal=True)

    def test_forward_update_attempts_every_target(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.remotes = {'first': 'url-1', 'second': 'url-2', 'third': 'url-3'}
            checkout.forwarding = [('origin', ['first', 'second', 'third'])]

            attempted = []

            # 'first' reports False the way an already-up-to-date target does.
            def record(branch=None, tag=None, remote=None, track=False, dest_branch=None):
                attempted.append(remote)
                return remote != 'first'

            checkout.push_update = record
            checkout.forward_update(branch='main', remote='origin')

            self.assertEqual(attempted, ['first', 'second', 'third'])

    def test_push_update_reports_nothing_to_push(self):
        from reporelaypy.checkout import NOTHING_TO_PUSH
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            self.assertIs(checkout.push_update(branch='main', remote=None), NOTHING_TO_PUSH)
            self.assertIs(checkout.push_update(branch='main', remote='fork'), NOTHING_TO_PUSH)

    def test_forward_update_succeeds_when_all_targets_current(self):
        from reporelaypy.checkout import NOTHING_TO_PUSH
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [('origin', ['a', 'b'])]
            checkout.push_update = lambda **kwargs: NOTHING_TO_PUSH
            self.assertTrue(checkout.forward_update(branch='main', remote='origin'))

    def test_forwarding_targets_first_match_only(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [
                ('origin:main', ['mirror-a:renamed']),
                ('origin', ['mirror-a', 'mirror-c']),
            ]
            # The specific rule wins and the general one must not also apply.
            self.assertEqual(
                checkout.forwarding_targets(remote='origin', branch='main'),
                [('mirror-a', 'renamed')],
            )
            self.assertEqual(
                checkout.forwarding_targets(remote='origin', branch='other'),
                [('mirror-a', None), ('mirror-c', None)],
            )

    def test_forwarding_targets_unmatched_remote_goes_nowhere(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [('origin', ['mirror-a'])]
            self.assertEqual(checkout.forwarding_targets(remote='mirror-d', branch='main'), [])

    def test_remote_sha_parses_ls_remote(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)

            captured = []

            def fake_run(args, **kwargs):
                captured.append(args)

                class Result(object):
                    returncode = 0
                    stdout = 'abc123\trefs/heads/main\n'

                return Result()

            import reporelaypy.checkout as module
            original, module.run = module.run, fake_run
            try:
                self.assertEqual(checkout.remote_sha('main', 'mirror-a'), 'abc123')
            finally:
                module.run = original

            self.assertIn('ls-remote', captured[0])
            self.assertIn('mirror-a', captured[0])

    def _remote_sha_with(self, checkout, fake_run):
        """Call remote_sha with `run` stubbed out."""
        import reporelaypy.checkout as module
        original, module.run = module.run, fake_run
        try:
            return checkout.remote_sha('main', 'mirror-a')
        finally:
            module.run = original

    def test_remote_sha_queries_with_a_timeout(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            seen = {}

            def fake_run(args, **kwargs):
                seen.update(kwargs)

                class Result(object):
                    returncode = 0
                    stdout = ''

                return Result()

            self.assertIsNone(self._remote_sha_with(checkout, fake_run))
            self.assertEqual(seen.get('timeout'), Checkout.REMOTE_QUERY_TIMEOUT)

    def test_remote_sha_raises_when_query_times_out(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)

            def fake_run(args, **kwargs):
                raise subprocess.TimeoutExpired(args, kwargs.get('timeout'))

            with self.assertRaises(Checkout.Exception):
                self._remote_sha_with(checkout, fake_run)

    def test_remote_sha_raises_when_query_fails(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)

            def fake_run(args, **kwargs):
                class Result(object):
                    returncode = 128
                    stdout = ''

                return Result()

            with self.assertRaises(Checkout.Exception):
                self._remote_sha_with(checkout, fake_run)

    def test_reconcile_does_not_push_to_unreachable_mirror(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [('origin', ['unreachable', 'reachable'])]
            checkout.fetch = lambda remote=None: True
            checkout.update_for = lambda branch=None, remote=None, track=False: True
            checkout.local_sha = lambda branch: 'newsha'

            def remote_sha(branch, remote):
                if remote == 'unreachable':
                    raise Checkout.Exception('cannot reach {}'.format(remote))
                return 'oldsha'

            checkout.remote_sha = remote_sha
            pushed = []
            checkout.push_update = lambda **kwargs: pushed.append(kwargs.get('remote')) or True

            results = checkout.reconcile(branches=['main'])

            # The unreachable mirror is never pushed to, and the one after it still is.
            self.assertEqual(pushed, ['reachable'])
            self.assertEqual(results[('unreachable', 'main')], 'failed')
            self.assertEqual(results[('reachable', 'main')], 'pushed')

    def test_reconcile_pushes_only_lagging_mirrors(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [('origin', ['current', 'behind'])]

            checkout.fetch = lambda remote=None: True
            checkout.update_for = lambda branch=None, remote=None, track=False: True
            checkout.local_sha = lambda branch: 'newsha'
            checkout.remote_sha = lambda branch, remote: (
                'newsha' if remote == 'current' else 'oldsha')

            pushed = []
            checkout.push_update = lambda **kwargs: pushed.append(kwargs.get('remote'))

            results = checkout.reconcile(branches=['main'])

            self.assertEqual(pushed, ['behind'])
            self.assertEqual(results[('behind', 'main')], 'pushed')
            self.assertEqual(results[('current', 'main')], 'current')

    def test_reconcile_honours_rename_and_does_not_touch_unrouted_remotes(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.remotes = {
                'mirror-a': 'url-1', 'mirror-b': 'url-2', 'mirror-d': 'url-3',
            }
            checkout.forwarding = [
                ('origin:release', ['mirror-a', 'mirror-b:renamed']),
                ('origin', ['mirror-a', 'mirror-b', 'mirror-d']),
            ]
            checkout.fetch = lambda remote=None: True
            checkout.update_for = lambda branch=None, remote=None, track=False: True
            checkout.local_sha = lambda branch: 'newsha'

            queried = []

            def remote_sha(branch, remote):
                queried.append((remote, branch))
                return 'oldsha'

            checkout.remote_sha = remote_sha

            pushed = []
            checkout.push_update = lambda **kwargs: pushed.append(
                (kwargs.get('remote'), kwargs.get('dest_branch')))

            results = checkout.reconcile(branches=['release'])

            # 'mirror-d' is never consulted or pushed for this branch.
            self.assertNotIn('mirror-d', [remote for remote, _ in queried])
            self.assertNotIn('mirror-d', [remote for remote, _ in pushed])
            # The renaming target is checked and pushed under its destination name.
            self.assertIn(('mirror-b', 'renamed'), queried)
            self.assertIn(('mirror-b', 'renamed'), pushed)
            self.assertEqual(results[('mirror-b', 'renamed')], 'pushed')
            self.assertEqual(results[('mirror-a', 'release')], 'pushed')

    def test_reconcile_reports_absent_ref(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [('origin', ['gone'])]
            checkout.fetch = lambda remote=None: True
            checkout.update_for = lambda branch=None, remote=None, track=False: True
            checkout.local_sha = lambda branch: 'newsha'
            checkout.remote_sha = lambda branch, remote: None
            checkout.push_update = lambda **kwargs: True

            results = checkout.reconcile(branches=['main'])
            self.assertEqual(results[('gone', 'main')], 'pushed')

    def test_reconcile_skips_when_source_unknown(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [('origin', ['a'])]
            checkout.fetch = lambda remote=None: True
            checkout.update_for = lambda branch=None, remote=None, track=False: True
            checkout.local_sha = lambda branch: None       # cannot resolve upstream
            checkout.push_update = lambda **kwargs: self.fail('must not push')

            results = checkout.reconcile(branches=['main'])
            self.assertEqual(results['main'], 'source-unknown')

    def test_reconcile_reports_branch_with_no_destinations(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo:
            checkout = Checkout(path=self.path, url=repo.remote, sentinal=False)
            checkout.forwarding = [('mirror-d', ['mirror-a'])]   # nothing matches origin
            checkout.fetch = lambda remote=None: True
            checkout.update_for = lambda branch=None, remote=None, track=False: True
            checkout.local_sha = lambda branch: 'newsha'
            checkout.push_update = lambda **kwargs: self.fail('must not push')

            results = checkout.reconcile(branches=['main'])
            self.assertEqual(results['main'], 'no-targets')
