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

from modules.buildbot import BuildBot

from modules.BeautifulSoup import BeautifulSoup

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
      <td align="center" class="Activity idle">idle</td>
    </table>
'''
    _expected_example_one_box_parsings = [
        {
            'builder_url': u'http://build.webkit.org/builders/Windows%20Debug%20%28Tests%29',
            'build_url': u'http://build.webkit.org/builders/Windows%20Debug%20%28Tests%29/builds/3693',
            'is_green': True,
            'name': u'Windows Debug (Tests)',
            'built_revision': 47380
        },
        {
            'builder_url': u'http://build.webkit.org/builders/SnowLeopard%20Intel%20Release',
            'is_green': False,
            'name': u'SnowLeopard Intel Release',
        },
        {
            'builder_url': u'http://build.webkit.org/builders/Qt%20Linux%20Release',
            'build_url': u'http://build.webkit.org/builders/Qt%20Linux%20Release/builds/654',
            'is_green': False,
            'name': u'Qt Linux Release',
            'built_revision': 47383
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

        example_builders = [
            { 'name': u'Leopard Debug (Build)', },
            { 'name': u'Leopard Debug (Tests)', },
            { 'name': u'Windows Release (Build)', },
            { 'name': u'Windows Debug (Tests)', },
            { 'name': u'Qt Linux Release', },
        ]
        name_regexps = [ 'Leopard', "Windows.*Build" ]
        expected_builders = [
            { 'name': u'Leopard Debug (Build)', },
            { 'name': u'Leopard Debug (Tests)', },
            { 'name': u'Windows Release (Build)', },
        ]

        # This test should probably be updated if the default regexp list changes
        self.assertEquals(buildbot.core_builder_names_regexps, name_regexps)

        builders = buildbot._builder_statuses_with_names_matching_regexps(example_builders, name_regexps)
        self.assertEquals(builders, expected_builders)

if __name__ == '__main__':
    unittest.main()
