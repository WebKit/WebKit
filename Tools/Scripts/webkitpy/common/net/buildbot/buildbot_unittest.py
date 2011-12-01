# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest

from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.buildbot import BuildBot, Builder, Build
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_failures
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup


class BuilderTest(unittest.TestCase):
    def _mock_test_result(self, testname):
        return test_results.TestResult(testname, [test_failures.FailureTextMismatch()])

    def _install_fetch_build(self, failure):
        def _mock_fetch_build(build_number):
            build = Build(
                builder=self.builder,
                build_number=build_number,
                revision=build_number + 1000,
                is_green=build_number < 4
            )
            results = [self._mock_test_result(testname) for testname in failure(build_number)]
            build._layout_test_results = LayoutTestResults(results)
            return build
        self.builder._fetch_build = _mock_fetch_build

    def setUp(self):
        self.buildbot = BuildBot()
        self.builder = Builder(u"Test Builder \u2661", self.buildbot)
        self._install_fetch_build(lambda build_number: ["test1", "test2"])

    def test_find_regression_window(self):
        regression_window = self.builder.find_regression_window(self.builder.build(10))
        self.assertEqual(regression_window.build_before_failure().revision(), 1003)
        self.assertEqual(regression_window.failing_build().revision(), 1004)

        regression_window = self.builder.find_regression_window(self.builder.build(10), look_back_limit=2)
        self.assertEqual(regression_window.build_before_failure(), None)
        self.assertEqual(regression_window.failing_build().revision(), 1008)

    def test_none_build(self):
        self.builder._fetch_build = lambda build_number: None
        regression_window = self.builder.find_regression_window(self.builder.build(10))
        self.assertEqual(regression_window.build_before_failure(), None)
        self.assertEqual(regression_window.failing_build(), None)

    def test_flaky_tests(self):
        self._install_fetch_build(lambda build_number: ["test1"] if build_number % 2 else ["test2"])
        regression_window = self.builder.find_regression_window(self.builder.build(10))
        self.assertEqual(regression_window.build_before_failure().revision(), 1009)
        self.assertEqual(regression_window.failing_build().revision(), 1010)

    def test_failure_and_flaky(self):
        self._install_fetch_build(lambda build_number: ["test1", "test2"] if build_number % 2 else ["test2"])
        regression_window = self.builder.find_regression_window(self.builder.build(10))
        self.assertEqual(regression_window.build_before_failure().revision(), 1003)
        self.assertEqual(regression_window.failing_build().revision(), 1004)

    def test_no_results(self):
        self._install_fetch_build(lambda build_number: ["test1", "test2"] if build_number % 2 else ["test2"])
        regression_window = self.builder.find_regression_window(self.builder.build(10))
        self.assertEqual(regression_window.build_before_failure().revision(), 1003)
        self.assertEqual(regression_window.failing_build().revision(), 1004)

    def test_failure_after_flaky(self):
        self._install_fetch_build(lambda build_number: ["test1", "test2"] if build_number > 6 else ["test3"])
        regression_window = self.builder.find_regression_window(self.builder.build(10))
        self.assertEqual(regression_window.build_before_failure().revision(), 1006)
        self.assertEqual(regression_window.failing_build().revision(), 1007)

    def test_find_blameworthy_regression_window(self):
        self.assertEqual(self.builder.find_blameworthy_regression_window(10).revisions(), [1004])
        self.assertEqual(self.builder.find_blameworthy_regression_window(10, look_back_limit=2), None)
        # Flakey test avoidance requires at least 2 red builds:
        self.assertEqual(self.builder.find_blameworthy_regression_window(4), None)
        self.assertEqual(self.builder.find_blameworthy_regression_window(4, avoid_flakey_tests=False).revisions(), [1004])
        # Green builder:
        self.assertEqual(self.builder.find_blameworthy_regression_window(3), None)

    def test_build_caching(self):
        self.assertEqual(self.builder.build(10), self.builder.build(10))

    def test_build_and_revision_for_filename(self):
        expectations = {
            "r47483 (1)/" : (47483, 1),
            "r47483 (1).zip" : (47483, 1),
        }
        for filename, revision_and_build in expectations.items():
            self.assertEqual(self.builder._revision_and_build_for_filename(filename), revision_and_build)

    def test_fetch_build(self):
        buildbot = BuildBot()
        builder = Builder(u"Test Builder \u2661", buildbot)

        def mock_fetch_build_dictionary(self, build_number):
            build_dictionary = {
                "sourceStamp": {
                    "revision": None,  # revision=None means a trunk build started from the force-build button on the builder page.
                    },
                "number": int(build_number),
                # Intentionally missing the 'results' key, meaning it's a "pass" build.
            }
            return build_dictionary
        buildbot._fetch_build_dictionary = mock_fetch_build_dictionary
        self.assertNotEqual(builder._fetch_build(1), None)


class BuildTest(unittest.TestCase):
    def test_layout_test_results(self):
        buildbot = BuildBot()
        builder = Builder(u"Foo Builder (test)", buildbot)
        build = Build(builder, None, None, None)
        build._fetch_file_from_results = lambda file_name: None
        # Test that layout_test_results() returns None if the fetch fails.
        self.assertEqual(build.layout_test_results(), None)


class BuildBotTest(unittest.TestCase):

    _example_one_box_status = '''
    <table>
    <tr>
    <td class="box"><a href="builders/Windows%20Debug%20%28Tests%29">Windows Debug (Tests)</a></td>
      <td align="center" class="LastBuild box success"><a href="builders/Windows%20Debug%20%28Tests%29/builds/3693">47380</a><br />build<br />successful</td>
      <td align="center" class="Activity building">building<br />ETA in<br />~ 14 mins<br />at 13:40</td>
    <tr>
    <td class="box"><a href="builders/SnowLeopard%20Intel%20Release">SnowLeopard Intel Release</a></td>
      <td class="LastBuild box" >no build</td>
      <td align="center" class="Activity building">building<br />< 1 min</td>
    <tr>
    <td class="box"><a href="builders/Qt%20Linux%20Release">Qt Linux Release</a></td>
      <td align="center" class="LastBuild box failure"><a href="builders/Qt%20Linux%20Release/builds/654">47383</a><br />failed<br />compile-webkit</td>
      <td align="center" class="Activity idle">idle<br />3 pending</td>
    <tr>
    <td class="box"><a href="builders/Qt%20Windows%2032-bit%20Debug">Qt Windows 32-bit Debug</a></td>
      <td align="center" class="LastBuild box failure"><a href="builders/Qt%20Windows%2032-bit%20Debug/builds/2090">60563</a><br />failed<br />failed<br />slave<br />lost</td>
      <td align="center" class="Activity building">building<br />ETA in<br />~ 5 mins<br />at 08:25</td>
    </table>
'''
    _expected_example_one_box_parsings = [
        {
            'is_green': True,
            'build_number' : 3693,
            'name': u'Windows Debug (Tests)',
            'built_revision': 47380,
            'activity': 'building',
            'pending_builds': 0,
        },
        {
            'is_green': False,
            'build_number' : None,
            'name': u'SnowLeopard Intel Release',
            'built_revision': None,
            'activity': 'building',
            'pending_builds': 0,
        },
        {
            'is_green': False,
            'build_number' : 654,
            'name': u'Qt Linux Release',
            'built_revision': 47383,
            'activity': 'idle',
            'pending_builds': 3,
        },
        {
            'is_green': True,
            'build_number' : 2090,
            'name': u'Qt Windows 32-bit Debug',
            'built_revision': 60563,
            'activity': 'building',
            'pending_builds': 0,
        },
    ]

    def test_status_parsing(self):
        buildbot = BuildBot()

        soup = BeautifulSoup(self._example_one_box_status)
        status_table = soup.find("table")
        input_rows = status_table.findAll('tr')

        for x in range(len(input_rows)):
            status_row = input_rows[x]
            expected_parsing = self._expected_example_one_box_parsings[x]

            builder = buildbot._parse_builder_status_from_row(status_row)

            # Make sure we aren't parsing more or less than we expect
            self.assertEquals(builder.keys(), expected_parsing.keys())

            for key, expected_value in expected_parsing.items():
                self.assertEquals(builder[key], expected_value, ("Builder %d parse failure for key: %s: Actual='%s' Expected='%s'" % (x, key, builder[key], expected_value)))

    def test_core_builder_methods(self):
        buildbot = BuildBot()

        # Override builder_statuses function to not touch the network.
        def example_builder_statuses(): # We could use instancemethod() to bind 'self' but we don't need to.
            return BuildBotTest._expected_example_one_box_parsings
        buildbot.builder_statuses = example_builder_statuses

        buildbot.core_builder_names_regexps = [ 'Leopard', "Windows.*Build" ]
        self.assertEquals(buildbot.red_core_builders_names(), [])
        self.assertTrue(buildbot.core_builders_are_green())

        buildbot.core_builder_names_regexps = [ 'SnowLeopard', 'Qt' ]
        self.assertEquals(buildbot.red_core_builders_names(), [ u'SnowLeopard Intel Release', u'Qt Linux Release' ])
        self.assertFalse(buildbot.core_builders_are_green())

    def test_builder_name_regexps(self):
        buildbot = BuildBot()

        # For complete testing, this list should match the list of builders at build.webkit.org:
        example_builders = [
            {'name': u'Leopard Intel Release (Build)', },
            {'name': u'Leopard Intel Release (Tests)', },
            {'name': u'Leopard Intel Debug (Build)', },
            {'name': u'Leopard Intel Debug (Tests)', },
            {'name': u'SnowLeopard Intel Release (Build)', },
            {'name': u'SnowLeopard Intel Release (Tests)', },
            {'name': u'SnowLeopard Intel Release (WebKit2 Tests)', },
            {'name': u'SnowLeopard Intel Leaks', },
            {'name': u'Windows Release (Build)', },
            {'name': u'Windows 7 Release (Tests)', },
            {'name': u'Windows Debug (Build)', },
            {'name': u'Windows XP Debug (Tests)', },
            {'name': u'Windows 7 Release (WebKit2 Tests)', },
            {'name': u'GTK Linux 32-bit Release', },
            {'name': u'GTK Linux 64-bit Release', },
            {'name': u'GTK Linux 64-bit Debug', },
            {'name': u'Qt Linux Release', },
            {'name': u'Qt Linux Release minimal', },
            {'name': u'Qt Linux ARMv7 Release', },
            {'name': u'Qt Windows 32-bit Release', },
            {'name': u'Qt Windows 32-bit Debug', },
            {'name': u'Chromium Android Release', },
            {'name': u'Chromium Win Release', },
            {'name': u'Chromium Win Release (Tests)', },
            {'name': u'Chromium Mac Release', },
            {'name': u'Chromium Mac Release (Tests)', },
            {'name': u'Chromium Linux Release', },
            {'name': u'Chromium Linux Release (Tests)', },
            {'name': u'Leopard Intel Release (NRWT)', },
            {'name': u'SnowLeopard Intel Release (NRWT)', },
            {'name': u'New run-webkit-tests', },
            {'name': u'WinCairo Debug (Build)', },
            {'name': u'WinCE Release (Build)', },
            {'name': u'EFL Linux Release (Build)', },
        ]
        name_regexps = [
            "SnowLeopard.*Build",
            "SnowLeopard.*\(Test",
            "SnowLeopard.*\(WebKit2 Test",
            "Leopard.*\((?:Build|Test)",
            "Windows.*Build",
            "Windows.*\(Test",
            "WinCE",
            "EFL",
            "GTK.*32",
            "GTK.*64",
            "Qt",
            "Chromium.*(Mac|Linux|Win).*Release$",
            "Chromium.*(Mac|Linux|Win).*Release.*\(Tests",
        ]
        expected_builders = [
            {'name': u'Leopard Intel Release (Build)', },
            {'name': u'Leopard Intel Release (Tests)', },
            {'name': u'Leopard Intel Debug (Build)', },
            {'name': u'Leopard Intel Debug (Tests)', },
            {'name': u'SnowLeopard Intel Release (Build)', },
            {'name': u'SnowLeopard Intel Release (Tests)', },
            {'name': u'SnowLeopard Intel Release (WebKit2 Tests)', },
            {'name': u'Windows Release (Build)', },
            {'name': u'Windows 7 Release (Tests)', },
            {'name': u'Windows Debug (Build)', },
            {'name': u'Windows XP Debug (Tests)', },
            {'name': u'GTK Linux 32-bit Release', },
            {'name': u'GTK Linux 64-bit Release', },
            {'name': u'GTK Linux 64-bit Debug', },
            {'name': u'Qt Linux Release', },
            {'name': u'Qt Linux Release minimal', },
            {'name': u'Qt Linux ARMv7 Release', },
            {'name': u'Qt Windows 32-bit Release', },
            {'name': u'Qt Windows 32-bit Debug', },
            {'name': u'Chromium Win Release', },
            {'name': u'Chromium Win Release (Tests)', },
            {'name': u'Chromium Mac Release', },
            {'name': u'Chromium Mac Release (Tests)', },
            {'name': u'Chromium Linux Release', },
            {'name': u'Chromium Linux Release (Tests)', },
            {'name': u'WinCE Release (Build)', },
            {'name': u'EFL Linux Release (Build)', },
        ]

        # This test should probably be updated if the default regexp list changes
        self.assertEquals(buildbot.core_builder_names_regexps, name_regexps)

        builders = buildbot._builder_statuses_with_names_matching_regexps(example_builders, name_regexps)
        self.assertEquals(builders, expected_builders)

    def test_builder_with_name(self):
        buildbot = BuildBot()

        builder = buildbot.builder_with_name("Test Builder")
        self.assertEqual(builder.name(), "Test Builder")
        self.assertEqual(builder.url(), "http://build.webkit.org/builders/Test%20Builder")
        self.assertEqual(builder.url_encoded_name(), "Test%20Builder")
        self.assertEqual(builder.results_url(), "http://build.webkit.org/results/Test%20Builder")

        # Override _fetch_build_dictionary function to not touch the network.
        def mock_fetch_build_dictionary(self, build_number):
            build_dictionary = {
                "sourceStamp": {
                    "revision" : 2 * build_number,
                    },
                "number" : int(build_number),
                "results" : build_number % 2, # 0 means pass
            }
            return build_dictionary
        buildbot._fetch_build_dictionary = mock_fetch_build_dictionary

        build = builder.build(10)
        self.assertEqual(build.builder(), builder)
        self.assertEqual(build.url(), "http://build.webkit.org/builders/Test%20Builder/builds/10")
        self.assertEqual(build.results_url(), "http://build.webkit.org/results/Test%20Builder/r20%20%2810%29")
        self.assertEqual(build.revision(), 20)
        self.assertEqual(build.is_green(), True)

        build = build.previous_build()
        self.assertEqual(build.builder(), builder)
        self.assertEqual(build.url(), "http://build.webkit.org/builders/Test%20Builder/builds/9")
        self.assertEqual(build.results_url(), "http://build.webkit.org/results/Test%20Builder/r18%20%289%29")
        self.assertEqual(build.revision(), 18)
        self.assertEqual(build.is_green(), False)

        self.assertEqual(builder.build(None), None)

    _example_directory_listing = '''
<h1>Directory listing for /results/SnowLeopard Intel Leaks/</h1>

<table>
        <tr class="alt">
            <th>Filename</th>
            <th>Size</th>
            <th>Content type</th>
            <th>Content encoding</th>
        </tr>
<tr class="directory ">
    <td><a href="r47483%20%281%29/"><b>r47483 (1)/</b></a></td>
    <td><b></b></td>
    <td><b>[Directory]</b></td>
    <td><b></b></td>
</tr>
<tr class="file alt">
    <td><a href="r47484%20%282%29.zip">r47484 (2).zip</a></td>
    <td>89K</td>
    <td>[application/zip]</td>
    <td></td>
</tr>
'''
    _expected_files = [
        {
            "filename" : "r47483 (1)/",
            "size" : "",
            "type" : "[Directory]",
            "encoding" : "",
        },
        {
            "filename" : "r47484 (2).zip",
            "size" : "89K",
            "type" : "[application/zip]",
            "encoding" : "",
        },
    ]

    def test_parse_build_to_revision_map(self):
        buildbot = BuildBot()
        files = buildbot._parse_twisted_directory_listing(self._example_directory_listing)
        self.assertEqual(self._expected_files, files)

    # Revision, is_green
    # Ordered from newest (highest number) to oldest.
    fake_builder1 = [
        [2, False],
        [1, True],
    ]
    fake_builder2 = [
        [2, False],
        [1, True],
    ]
    fake_builders = [
        fake_builder1,
        fake_builder2,
    ]
    def _build_from_fake(self, fake_builder, index):
        if index >= len(fake_builder):
            return None
        fake_build = fake_builder[index]
        build = Build(
            builder=fake_builder,
            build_number=index,
            revision=fake_build[0],
            is_green=fake_build[1],
        )
        def mock_previous_build():
            return self._build_from_fake(fake_builder, index + 1)
        build.previous_build = mock_previous_build
        return build

    def _fake_builds_at_index(self, index):
        return [self._build_from_fake(builder, index) for builder in self.fake_builders]

    def test_last_green_revision(self):
        buildbot = BuildBot()
        def mock_builds_from_builders(only_core_builders):
            return self._fake_builds_at_index(0)
        buildbot._latest_builds_from_builders = mock_builds_from_builders
        self.assertEqual(buildbot.last_green_revision(), 1)

    def _fetch_build(self, build_number):
        if build_number == 5:
            return "correct build"
        return "wrong build"

    def _fetch_revision_to_build_map(self):
        return {'r5': 5, 'r2': 2, 'r3': 3}

    def test_latest_cached_build(self):
        b = Builder('builder', BuildBot())
        b._fetch_build = self._fetch_build
        b._fetch_revision_to_build_map = self._fetch_revision_to_build_map
        self.assertEquals("correct build", b.latest_cached_build())

    def results_url(self):
        return "some-url"

    def test_results_zip_url(self):
        b = Build(None, 123, 123, False)
        b.results_url = self.results_url
        self.assertEquals("some-url.zip", b.results_zip_url())


if __name__ == '__main__':
    unittest.main()
