# Copyright (C) 2019 Apple Inc. All rights reserved.
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

import time

from fakeredis import FakeStrictRedis
from resultsdbpy.controller.commit import Commit
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from resultsdbpy.view.view_routes_unittest import WebSiteTestCase
from selenium.webdriver.support.select import Select


class CommitViewUnittest(WebSiteTestCase):
    def register_all_commits(self, client):
        for repo in [MockStashRepository.safari(), MockSVNRepository.webkit()]:
            for commits in repo.commits.values():
                for commit in commits:
                    self.assertEqual(200, client.post(self.URL + '/api/commits/register', data=Commit.Encoder().default(commit)).status_code)

    def unpack_commit_table(self, commit_table):
        headers = commit_table.find_elements_by_tag_name('th')
        repos = [header.text for header in headers if header.text]
        indecies = [1 for _ in range(len(headers))]
        commits = {repo: [] for repo in repos}

        rows = commit_table.find_element_by_tag_name('tbody').find_elements_by_tag_name('tr')
        for row in rows:
            indecies = [i - 1 for i in indecies]
            cells = row.find_elements_by_tag_name('td')
            i = 0
            for cell in cells:
                while i < len(indecies) and indecies[i] != 0:
                    i += 1
                self.assertLess(i, len(indecies))
                indecies[i] = int(cell.get_attribute('rowspan') or 1)
                if i == 0 or not cell.text:
                    continue
                commits[repos[i - 1]].append(cell)
        return commits

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_drawer(self, driver, **kwargs):
        driver.get(self.URL + '/commits')
        time.sleep(.2)
        self.assertNotIn('display', driver.find_element_by_class_name('drawer').get_attribute('class'))

        self.toggle_drawer(driver, assert_displayed=True)
        self.toggle_drawer(driver, assert_displayed=False)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_commit_table(self, driver, client, **kwargs):
        self.register_all_commits(client)
        driver.get(self.URL + '/commits')

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        self.assertEqual(2, len(commits.keys()))
        self.assertIn('safari', commits)
        self.assertIn('webkit', commits)

        self.assertEqual(5, len(commits['safari']))
        self.assertEqual(5, len(commits['webkit']))

        rows = commit_table.find_element_by_tag_name('tbody').find_elements_by_tag_name('tr')
        self.assertNotEqual(
            rows[1].find_elements_by_tag_name('td')[0].text,
            rows[2].find_elements_by_tag_name('td')[0].text,
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_commit(self, driver, client, **kwargs):
        self.register_all_commits(client)
        driver.get(self.URL + '/commits')

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        links = commits['safari'][0].find_elements_by_tag_name('a')
        for link in links:
            if link.text != 'More Info':
                link.click()
                break

        time.sleep(.5)

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        self.assertEqual(2, len(commits.keys()))
        self.assertIn('safari', commits)
        self.assertIn('webkit', commits)

        self.assertEqual(1, len(commits['safari']))
        self.assertEqual(5, len(commits['webkit']))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_radar_strings(self, driver, client, **kwargs):
        self.register_all_commits(client)
        driver.get(self.URL + '/commits?id=336610a4')

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        changelog = commits['safari'][0].find_element_by_tag_name('div')

        radar = '<rdar://problem/99999999>'
        self.assertEqual(changelog.text[:len(radar)], radar)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_range_slider(self, driver, client, **kwargs):
        self.register_all_commits(client)
        driver.get(self.URL + '/commits')

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        self.assertEqual(2, len(commits.keys()))
        self.assertIn('safari', commits)
        self.assertIn('webkit', commits)

        self.assertEqual(5, len(commits['safari']))
        self.assertEqual(5, len(commits['webkit']))

        self.toggle_drawer(driver, assert_displayed=True)

        controls = self.find_input_with_name(driver, 'Limit:').find_elements_by_tag_name('input')
        self.assertEqual(3, len(controls))
        input = [control for control in controls if control.get_attribute('type') == 'number'][0]
        self.assertIsNotNone(input)

        input.clear()
        input.send_keys('3')

        self.toggle_drawer(driver, assert_displayed=False)

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        self.assertEqual(2, len(commits.keys()))
        self.assertIn('safari', commits)
        self.assertIn('webkit', commits)

        self.assertEqual(3, len(commits['safari']))
        self.assertEqual(3, len(commits['webkit']))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_one_line_switch(self, driver, client, **kwargs):
        self.register_all_commits(client)
        driver.get(self.URL + '/commits?id=7be40842')

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        line_1 = u'Change 4 \u2014 (Part 2) description.'
        line_2 = 'Reviewed by person.'

        commits = self.unpack_commit_table(commit_table)
        changelog = commits['safari'][0].find_element_by_tag_name('div')
        self.assertEqual(line_1, changelog.text)

        self.toggle_drawer(driver, assert_displayed=True)

        controls = self.find_input_with_name(driver, 'One-line:').find_elements_by_tag_name('span')
        self.assertEqual(1, len(controls))
        input = [control for control in controls if control.get_attribute('class') == 'slider'][0]
        self.assertIsNotNone(input)
        input.click()

        self.toggle_drawer(driver, assert_displayed=False)

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        changelog = commits['safari'][0].find_element_by_tag_name('div')
        self.assertEqual(line_1 + line_2, changelog.text)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_branch_selection(self, driver, client, **kwargs):
        self.register_all_commits(client)
        driver.get(self.URL + '/commits')

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        self.assertEqual(2, len(commits.keys()))
        self.assertIn('safari', commits)
        self.assertIn('webkit', commits)

        self.assertEqual(5, len(commits['safari']))
        self.assertEqual(5, len(commits['webkit']))

        self.toggle_drawer(driver, assert_displayed=True)

        controls = self.find_input_with_name(driver, 'Branch').find_elements_by_tag_name('select')
        self.assertEqual(1, len(controls))
        Select(controls[0]).select_by_visible_text('safari-606-branch')

        self.toggle_drawer(driver, assert_displayed=False)

        while not driver.find_elements_by_class_name('commit-table'):
            time.sleep(.1)
        commit_table = driver.find_element_by_class_name('commit-table')

        commits = self.unpack_commit_table(commit_table)
        self.assertEqual(2, len(commits.keys()))
        self.assertIn('safari', commits)
        self.assertIn('webkit', commits)

        self.assertEqual(2, len(commits['safari']))
        self.assertEqual(2, len(commits['webkit']))
