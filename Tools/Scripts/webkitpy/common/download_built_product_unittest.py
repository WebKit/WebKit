# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import contextlib
import io
import unittest
import urllib.error
import urllib.request

from webkitcorepy import OutputCapture
from webkitpy.common import download_built_product as dbp
from webkitscmpy import mocks

CONFIG = {
    'builders': [
        {'name': 'WPE-Linux-64-bit-Release-Build', 'platform': 'wpe', 'configuration': 'release', 'architectures': ['x86_64'], 'triggers': ['wpe-tests']},
        {'name': 'WPE-Linux-64-bit-Release-Tests', 'platform': 'wpe', 'configuration': 'release', 'architectures': ['x86_64']},
        {'name': 'Apple-Tahoe-Debug-Build', 'platform': 'mac-tahoe', 'configuration': 'debug', 'architectures': ['x86_64', 'arm64'], 'triggers': ['tahoe-tests']},
        {'name': 'Apple-iOS-26-Simulator-Release-Build', 'platform': 'ios-simulator-26', 'configuration': 'release', 'architectures': ['x86_64', 'arm64'], 'triggers': ['ios-tests']},
        {'name': 'WPE-Linux-RPi4-64bits-Mesa-Release-Perf-Build', 'platform': 'wpe-rpi4-64bits-mesa', 'configuration': 'release', 'architectures': ['arm64'], 'triggers': ['rpi4-tests'], 'additionalArguments': ['--cross-target=rpi4-64bits-mesa']},
    ],
}


def build(number, archive_revision, results=0, git_hash='0123456789abcdef', forced_hash=None):
    properties = {'archive_revision': [archive_revision, 'Unknown'], 'got_revision': [git_hash, 'CheckOutSource']}
    if forced_hash:
        properties['user_provided_git_hash'] = [forced_hash, 'Force Build Form']
    return {'number': number, 'results': results, 'complete_at': 1794000000, 'properties': properties}


class FakeBuildbot(object):
    """Answers the builds API from a list of builds, newest first, in pages."""

    def __init__(self, builds):
        self.builds = builds
        self.urls = []

    def __call__(self, url):
        self.urls.append(url)
        offset = int(url.split('offset=')[1].split('&')[0])
        return {'builds': self.builds[offset:offset + dbp.BUILDS_PER_REQUEST]}


class DownloadBuiltProductTest(unittest.TestCase):

    def setUp(self):
        self.configurations = dbp.load_configurations(CONFIG)

    def test_configurations_are_the_builders_that_upload(self):
        self.assertEqual(sorted(self.configurations), [
            'ios-simulator-26-x86_64-arm64-release',
            'mac-tahoe-x86_64-arm64-debug',
            'wpe-rpi4-64bits-mesa-arm64-release',
            'wpe-x86_64-release',
        ])

    def test_commands_match_the_bots(self):
        mac = self.configurations['mac-tahoe-x86_64-arm64-debug']
        self.assertEqual(mac.archive_url('325861@main'), 'https://s3-us-west-2.amazonaws.com/archives.webkit.org/mac-tahoe-x86_64-arm64-debug/325861@main.zip')
        self.assertEqual(mac.download_command('325861@main')[2:], ['--platform=mac', '--debug', mac.archive_url('325861@main')])
        self.assertEqual(mac.extract_command()[2:], ['--platform=mac-tahoe', '--debug', 'extract'])

        ios = self.configurations['ios-simulator-26-x86_64-arm64-release']
        self.assertEqual(ios.download_command('1@main')[2], '--platform=ios')
        self.assertEqual(ios.extract_command()[2], '--platform=ios-simulator-26')

        rpi = self.configurations['wpe-rpi4-64bits-mesa-arm64-release']
        self.assertEqual(rpi.download_command('1@main')[2], '--platform=wpe')
        self.assertEqual(rpi.extract_command()[2:], ['--platform=wpe-rpi4-64bits-mesa', '--release', 'extract'])

    def test_builder_name_is_quoted_in_the_api_url(self):
        fetch = FakeBuildbot([])
        list(dbp.products(self.configurations['wpe-x86_64-release'], fetch=fetch))
        self.assertIn('/api/v2/builders/WPE-Linux-64-bit-Release-Build/builds?', fetch.urls[0])

    def test_products_skip_failed_builds_and_page(self):
        builds = [build(1000 - index, f'{325900 - index}@main') for index in range(250)]
        builds[1]['results'] = 2  # FAILURE: nothing was uploaded
        fetch = FakeBuildbot(builds)
        found = list(dbp.products(self.configurations['wpe-x86_64-release'], fetch=fetch))
        self.assertEqual(len(found), 249)
        self.assertEqual(found[0].identifier, 325900)
        self.assertEqual(found[1].identifier, 325898)
        self.assertEqual(len(fetch.urls), 4)

    def test_forced_builds_use_the_forced_hash(self):
        # The forced build checked out hash325990 first, then the forced hash325970, which is what it archived as 325970@main.
        fetch = FakeBuildbot([build(1001, '325970@main', git_hash='hash325990', forced_hash='hash325970'), build(1000, '325990@main', git_hash='hash325990')])
        found = list(dbp.products(self.configurations['wpe-x86_64-release'], fetch=fetch))
        self.assertEqual([product.git_hash for product in found], ['hash325970', 'hash325990'])
        product, older, newer = dbp.find_product(iter(found), None, git_hash='hash325990')
        self.assertEqual(product.build_number, 1000)

    def test_find_product_by_abbreviated_hash(self):
        candidates = [dbp.Product('325990@main', '0123abc', 1, None)]
        product, older, newer = dbp.find_product(iter(candidates), None, git_hash='0123abcdef0123')
        self.assertEqual(product.archive_revision, '325990@main')

    def test_find_release_branch_product(self):
        candidates = [dbp.Product('305877.1199@webkitglib/2.52', 'hash1199', 2, None), dbp.Product('305877.1198@webkitglib/2.52', 'hash1198', 1, None)]
        product, older, newer = dbp.find_product(iter(candidates), None, name='305877.1198@webkitglib/2.52')
        self.assertEqual(product.git_hash, 'hash1198')
        # A commit on the branch, by hash, with the main commit the branch is based on as the identifier.
        product, older, newer = dbp.find_product(iter(candidates), 305877, git_hash='hash1198', name='305877.1198@webkitglib/2.52')
        self.assertEqual(product.git_hash, 'hash1198')
        product, older, newer = dbp.find_product(iter(candidates), 305877, git_hash='hash1100', name='305877.1100@webkitglib/2.52')
        self.assertEqual((product, older, newer), (None, None, None))

    def test_find_exact_product(self):
        candidates = [dbp.Product(f'{n}@main', f'hash{n}', n, None) for n in (325990, 325985, 325980)]
        product, older, newer = dbp.find_product(iter(candidates), 325985)
        self.assertEqual(product.archive_revision, '325985@main')

    def test_find_product_by_hash(self):
        candidates = [dbp.Product('0123abcd', '0123abcdef', 1, None)]
        product, older, newer = dbp.find_product(iter(candidates), 325985, git_hash='0123abcdef')
        self.assertEqual(product.archive_revision, '0123abcd')

    def test_find_nearest_products_when_there_is_no_exact_one(self):
        candidates = [dbp.Product(f'{n}@main', f'hash{n}', n, None) for n in (325990, 325985, 325980)]
        product, older, newer = dbp.find_product(iter(candidates), 325983)
        self.assertIsNone(product)
        self.assertEqual(older.identifier, 325980)
        self.assertEqual(newer.identifier, 325985)

        product, older, newer = dbp.find_product(iter(candidates), 325999)
        self.assertEqual((product, older.identifier, newer), (None, 325990, None))

    def test_find_product_in_builds_out_of_revision_order(self):
        # A forced build of an older revision, and builds of parallel workers that finished out of order.
        candidates = [dbp.Product(f'{n}@main', f'hash{n}', None, None) for n in (325990, 325970, 325985, 325980, 324507, 324509)]
        self.assertEqual(dbp.find_product(iter(candidates), 325985)[0].identifier, 325985)
        self.assertEqual(dbp.find_product(iter(candidates), 324509)[0].identifier, 324509)
        product, older, newer = dbp.find_product(iter(candidates), 325983)
        self.assertEqual((product, older.identifier, newer.identifier), (None, 325980, 325985))

    def test_search_stops_past_an_older_product(self):
        consumed = []

        def candidates():
            for n in [325990] + list(range(325900, 325850, -1)) + [325950]:
                consumed.append(n)
                yield dbp.Product(f'{n}@main', None, None, None)
        product, older, newer = dbp.find_product(candidates(), 325950)
        self.assertEqual((product, older.identifier, newer.identifier), (None, 325900, 325990))
        # The newer one, the first older one, the ones searched past it, and the one read when the limit is reached.
        self.assertEqual(len(consumed), 3 + dbp.PRODUCTS_TO_SEARCH_PAST_AN_OLDER_ONE)

    def test_identifiers_need_no_repository(self):
        self.assertEqual(dbp.resolve_revision('325980'), (325980, None, '325980@main', None))
        self.assertEqual(dbp.resolve_revision('325980@main'), (325980, None, '325980@main', None))

    def test_branch_identifiers(self):
        with OutputCapture(), mocks.local.Git('/.invalid-git') as repository:
            branch_commit = repository.commits['eng/squash-branch'][-1]
            # A branch in the checkout resolves like its ref, so a local development branch falls back to the main commit it is based on.
            identifier, git_hash, name, note = dbp.resolve_revision('5.3@eng/squash-branch', repository_path='/.invalid-git')
            self.assertEqual((identifier, git_hash, name), (5, branch_commit.hash, '5.3@eng/squash-branch'))
            self.assertIn('using the commit it is based on, 5@main', note)
            # A branch that is not in the checkout, like a release branch never checked out, is looked for by name.
            self.assertEqual(dbp.resolve_revision('305877.1198@webkitglib/2.52', repository_path='/.invalid-git'), (None, None, '305877.1198@webkitglib/2.52', None))

    def test_only_403_and_404_mean_a_missing_archive(self):
        def urlopen(code):
            def raise_error(request, timeout):
                raise urllib.error.HTTPError(request.full_url, code, 'error', {}, None)
            return raise_error
        saved = urllib.request.urlopen
        try:
            for code in (403, 404):
                urllib.request.urlopen = urlopen(code)
                self.assertFalse(dbp.archive_exists('https://example.invalid/a.zip'))
            urllib.request.urlopen = urlopen(503)
            with self.assertRaises(urllib.error.HTTPError):
                dbp.archive_exists('https://example.invalid/a.zip')
        finally:
            urllib.request.urlopen = saved

    def test_arguments(self):
        self.assertEqual(dbp.parse_args(['--list']).config, None)
        args = dbp.parse_args(['--config', 'wpe-x86_64-release', '325980@main'])
        self.assertEqual((args.config, args.revision, args.list), ('wpe-x86_64-release', '325980@main', False))
        with self.assertRaises(SystemExit):
            dbp.parse_args([])
        with self.assertRaises(SystemExit):
            dbp.parse_args(['--config', 'wpe-x86_64-release', '--list', '325980'])
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            dbp.parse_args(['--config', 'wpe-x86_64-release', '--list', '--limit', '-1'])


class MainTest(unittest.TestCase):
    """main() with the network, git and the download tools replaced by fakes."""

    def setUp(self):
        self.commands = []
        self.saved = (dbp.archive_exists, dbp.resolve_revision)
        dbp.archive_exists = lambda url: True
        resolve_revision = dbp.resolve_revision
        dbp.resolve_revision = lambda revision: (326005, 'headhash', '326005@main', None) if revision is None else resolve_revision(revision)
        self.fetch = FakeBuildbot([build(59934, '325998@main', git_hash='6dfc108e1a4b4332'), build(59933, '325994@main')])

    def tearDown(self):
        dbp.archive_exists, dbp.resolve_revision = self.saved

    def run_main(self, argv, checked_out=(326005, 'headhash', '326005@main')):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stderr(err):
            status = dbp.main(argv, fetch=self.fetch, run=lambda command, cwd: self.commands.append(command) or 0, out=out, checked_out=lambda: checked_out)
        return status, out.getvalue(), err.getvalue()

    def test_last_downloads_the_newest_product_and_warns_about_the_checkout(self):
        status, out, err = self.run_main(['--config', 'wpe-x86_64-release', 'last'])
        self.assertEqual(status, 0)
        self.assertIn('The newest product of wpe-x86_64-release is 325998@main.', out)
        self.assertEqual(len(self.commands), 2)
        self.assertTrue(self.commands[0][-1].endswith('/wpe-x86_64-release/325998@main.zip'))
        self.assertEqual(self.commands[1][-1], 'extract')
        self.assertIn('WARNING: the product is 325998@main, but the checked-out commit is 326005@main', err)
        self.assertIn('git checkout 6dfc108e1a4b', err)

    def test_no_warning_when_the_product_matches_the_checkout(self):
        status, out, err = self.run_main(['--config', 'wpe-x86_64-release', 'last'], checked_out=(325998, None, '325998@main'))
        self.assertEqual(status, 0)
        self.assertNotIn('WARNING', err)

    def test_default_says_it_looks_for_the_checked_out_commit(self):
        status, out, err = self.run_main(['--config', 'wpe-x86_64-release'])
        self.assertEqual(status, 1)
        self.assertIn('looking for the product of the checked-out commit, 326005@main', out)
        self.assertIn('has no product for the checked-out commit, 326005@main', err)
        self.assertIn('Nearest older: 325998@main', err)
        self.assertIn('"last"', err)
        self.assertEqual(self.commands, [])

    def test_revision_older_than_the_builds_searched(self):
        status, out, err = self.run_main(['--config', 'wpe-x86_64-release', '325000@main'])
        self.assertEqual(status, 1)
        self.assertIn(f'has no product for 325000@main among the builds searched (up to its last {dbp.MAX_BUILDS_TO_SEARCH}).', err)
        self.assertIn('Nearest newer: 325994@main', err)
        self.assertNotIn('Nearest older', err)

    def test_release_branch_revision(self):
        self.fetch = FakeBuildbot([build(136, '305877.1199@webkitglib/2.52'), build(135, '305877.1198@webkitglib/2.52', git_hash='527bb1354575')])
        status, out, err = self.run_main(['--config', 'wpe-rpi4-64bits-mesa-arm64-release', '305877.1198@webkitglib/2.52'], checked_out=(305877, '527bb1354575', '305877.1198@webkitglib/2.52'))
        self.assertEqual(status, 0)
        self.assertTrue(self.commands[0][-1].endswith('/wpe-rpi4-64bits-mesa-arm64-release/305877.1198@webkitglib/2.52.zip'))
        self.assertNotIn('WARNING', err)

        status, out, err = self.run_main(['--config', 'wpe-rpi4-64bits-mesa-arm64-release', '305877.1100@webkitglib/2.52'])
        self.assertEqual(status, 1)
        self.assertIn('Run with --list', err)

    def test_branch_note_only_when_main_products_are_used(self):
        dbp.resolve_revision = lambda revision: (325994, 'branchhash', '325994.2@eng/feature', 'BRANCH NOTE')
        status, out, err = self.run_main(['--config', 'wpe-x86_64-release', 'eng/feature'])
        self.assertEqual(status, 0)
        self.assertIn('BRANCH NOTE', out)
        self.assertTrue(self.commands[0][-1].endswith('/325994@main.zip'))

        # A release-branch configuration has no products of main to fall back to.
        self.fetch = FakeBuildbot([build(136, '305877.1199@webkitglib/2.52')])
        status, out, err = self.run_main(['--config', 'wpe-rpi4-64bits-mesa-arm64-release', 'eng/feature'])
        self.assertEqual(status, 1)
        self.assertNotIn('BRANCH NOTE', out)

    def test_list_stops_reading_at_the_limit(self):
        # The first page has the 20 products asked for; the next request would fail, and is not needed.
        builds = [build(1000 - index, f'{325900 - index}@main', results=0 if index < 20 else 2) for index in range(dbp.BUILDS_PER_REQUEST)]
        fetch = FakeBuildbot(builds)

        def fetch_one_page(url):
            if fetch.urls:
                raise TimeoutError('The read operation timed out')
            return fetch(url)
        self.fetch = fetch_one_page
        status, out, err = self.run_main(['--config', 'wpe-x86_64-release', '--list', '--limit', '20'])
        self.assertEqual(status, 0)
        self.assertEqual(len([line for line in out.splitlines() if line.startswith('  ')]), 20)
        self.assertEqual(len(fetch.urls), 1)

        status, out, err = self.run_main(['--config', 'wpe-x86_64-release', '--list', '--limit', '0'])
        self.assertEqual(status, 0)
        self.assertEqual(len(fetch.urls), 1)

    def test_explicit_revision(self):
        status, out, err = self.run_main(['--config', 'wpe-x86_64-release', '325994@main'])
        self.assertEqual(status, 0)
        self.assertTrue(self.commands[0][-1].endswith('/325994@main.zip'))
        self.assertIn('WARNING: the product is 325994@main', err)


class ConfigTest(unittest.TestCase):

    def test_every_uploading_builder_has_its_own_configuration(self):
        # The script relies on each S3 directory belonging to a single builder.
        config = dbp.read_config()
        uploading = [builder for builder in config['builders'] if builder.get('triggers')]
        self.assertTrue(uploading)
        self.assertEqual(len(dbp.load_configurations(config)), len(uploading))
