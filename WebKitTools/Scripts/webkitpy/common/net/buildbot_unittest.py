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

from webkitpy.common.net.buildbot import BuildBot, Builder, Build, LayoutTestResults

from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup


class BuilderTest(unittest.TestCase):
    def _install_fetch_build(self, failure):
        def _mock_fetch_build(build_number):
            build = Build(
                builder=self.builder,
                build_number=build_number,
                revision=build_number + 1000,
                is_green=build_number < 4
            )
            build._layout_test_results = LayoutTestResults(
                "http://buildbot.example.com/foo", {
                    LayoutTestResults.fail_key: failure(build_number),
                })
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

    def test_blameworthy_revisions(self):
        self.assertEqual(self.builder.blameworthy_revisions(10), [1004])
        self.assertEqual(self.builder.blameworthy_revisions(10, look_back_limit=2), [])
        # Flakey test avoidance requires at least 2 red builds:
        self.assertEqual(self.builder.blameworthy_revisions(4), [])
        self.assertEqual(self.builder.blameworthy_revisions(4, avoid_flakey_tests=False), [1004])
        # Green builder:
        self.assertEqual(self.builder.blameworthy_revisions(3), [])

    def test_build_caching(self):
        self.assertEqual(self.builder.build(10), self.builder.build(10))

    def test_build_and_revision_for_filename(self):
        expectations = {
            "r47483 (1)/" : (47483, 1),
            "r47483 (1).zip" : (47483, 1),
        }
        for filename, revision_and_build in expectations.items():
            self.assertEqual(self.builder._revision_and_build_for_filename(filename), revision_and_build)


class LayoutTestResultsTest(unittest.TestCase):
    _example_results_html = """
<html>
<head>
<title>Layout Test Results</title>
</head>
<body>
<p>Tests that had stderr output:</p>
<table>
<tr>
<td><a href="/var/lib/buildbot/build/gtk-linux-64-release/build/LayoutTests/accessibility/aria-activedescendant-crash.html">accessibility/aria-activedescendant-crash.html</a></td>
<td><a href="accessibility/aria-activedescendant-crash-stderr.txt">stderr</a></td>
</tr>
<td><a href="/var/lib/buildbot/build/gtk-linux-64-release/build/LayoutTests/http/tests/security/canvas-remote-read-svg-image.html">http/tests/security/canvas-remote-read-svg-image.html</a></td>
<td><a href="http/tests/security/canvas-remote-read-svg-image-stderr.txt">stderr</a></td>
</tr>
</table><p>Tests that had no expected results (probably new):</p>
<table>
<tr>
<td><a href="/var/lib/buildbot/build/gtk-linux-64-release/build/LayoutTests/fast/repaint/no-caret-repaint-in-non-content-editable-element.html">fast/repaint/no-caret-repaint-in-non-content-editable-element.html</a></td>
<td><a href="fast/repaint/no-caret-repaint-in-non-content-editable-element-actual.txt">result</a></td>
</tr>
</table></body>
</html>
"""

    _expected_layout_test_results = {
        'Tests that had stderr output:' : [
            'accessibility/aria-activedescendant-crash.html'
        ],
        'Tests that had no expected results (probably new):' : [
            'fast/repaint/no-caret-repaint-in-non-content-editable-element.html'
        ]
    }
    def test_parse_layout_test_results(self):
        results = LayoutTestResults._parse_results_html(self._example_results_html)
        self.assertEqual(self._expected_layout_test_results, results)


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
            {'name': u'Tiger Intel Release', },
            {'name': u'Leopard Intel Release (Build)', },
            {'name': u'Leopard Intel Release (Tests)', },
            {'name': u'Leopard Intel Debug (Build)', },
            {'name': u'Leopard Intel Debug (Tests)', },
            {'name': u'SnowLeopard Intel Release (Build)', },
            {'name': u'SnowLeopard Intel Release (Tests)', },
            {'name': u'SnowLeopard Intel Release (WebKit2 Tests)', },
            {'name': u'SnowLeopard Intel Leaks', },
            {'name': u'Windows Release (Build)', },
            {'name': u'Windows Release (Tests)', },
            {'name': u'Windows Debug (Build)', },
            {'name': u'Windows Debug (Tests)', },
            {'name': u'GTK Linux 32-bit Release', },
            {'name': u'GTK Linux 32-bit Debug', },
            {'name': u'GTK Linux 64-bit Debug', },
            {'name': u'GTK Linux 64-bit Release', },
            {'name': u'Qt Linux Release', },
            {'name': u'Qt Linux Release minimal', },
            {'name': u'Qt Linux ARMv5 Release', },
            {'name': u'Qt Linux ARMv7 Release', },
            {'name': u'Qt Windows 32-bit Release', },
            {'name': u'Qt Windows 32-bit Debug', },
            {'name': u'Chromium Linux Release', },
            {'name': u'Chromium Mac Release', },
            {'name': u'Chromium Win Release', },
            {'name': u'Chromium Linux Release (Tests)', },
            {'name': u'Chromium Mac Release (Tests)', },
            {'name': u'Chromium Win Release (Tests)', },
            {'name': u'New run-webkit-tests', },
        ]
        name_regexps = [
            "SnowLeopard.*Build",
            "SnowLeopard.*\(Test",
            "Leopard",
            "Tiger",
            "Windows.*Build",
            "GTK.*32",
            "GTK.*64.*Debug",  # Disallow the 64-bit Release bot which is broken.
            "Qt",
            "Chromium.*Release$",
        ]
        expected_builders = [
            {'name': u'Tiger Intel Release', },
            {'name': u'Leopard Intel Release (Build)', },
            {'name': u'Leopard Intel Release (Tests)', },
            {'name': u'Leopard Intel Debug (Build)', },
            {'name': u'Leopard Intel Debug (Tests)', },
            {'name': u'SnowLeopard Intel Release (Build)', },
            {'name': u'SnowLeopard Intel Release (Tests)', },
            {'name': u'Windows Release (Build)', },
            {'name': u'Windows Debug (Build)', },
            {'name': u'GTK Linux 32-bit Release', },
            {'name': u'GTK Linux 32-bit Debug', },
            {'name': u'GTK Linux 64-bit Debug', },
            {'name': u'Qt Linux Release', },
            {'name': u'Qt Linux Release minimal', },
            {'name': u'Qt Linux ARMv5 Release', },
            {'name': u'Qt Linux ARMv7 Release', },
            {'name': u'Qt Windows 32-bit Release', },
            {'name': u'Qt Windows 32-bit Debug', },
            {'name': u'Chromium Linux Release', },
            {'name': u'Chromium Mac Release', },
            {'name': u'Chromium Win Release', },
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

        # Override _fetch_xmlrpc_build_dictionary function to not touch the network.
        def mock_fetch_xmlrpc_build_dictionary(self, build_number):
            build_dictionary = {
                "revision" : 2 * build_number,
                "number" : int(build_number),
                "results" : build_number % 2, # 0 means pass
            }
            return build_dictionary
        buildbot._fetch_xmlrpc_build_dictionary = mock_fetch_xmlrpc_build_dictionary

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


if __name__ == '__main__':
    unittest.main()
