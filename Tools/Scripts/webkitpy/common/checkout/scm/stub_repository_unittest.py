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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.common.system import filesystem_mock
from webkitpy.common.checkout.scm.stub_repository import StubRepository

mock_stub_repository_json = {'branch': 'trunk', 'id': '2738499'}

FAKE_FILES = {
    '/TestDirectory/test.txt': 'test',
    '/TestDirectory/TestDirectory2/checkout_information.json': '{ "branch": "trunk", "id": "2738499" }',
    '/TestDirectory/TestDirectory2/TestDirectory3/test2.txt': 'test',
}


class StubRepositoryTest(unittest.TestCase):

    @staticmethod
    def mock_host_for_stub_repository():
        host = filesystem_mock.MockFileSystem(files=FAKE_FILES)
        return host

    def test_in_working_directory(self):
        host = StubRepositoryTest.mock_host_for_stub_repository()
        self.assertTrue(StubRepository.in_working_directory(path=host.join(host.getcwd(), 'TestDirectory', 'TestDirectory2', 'TestDirectory3'), filesystem=host))

    def test_native_revision(self):
        host = StubRepositoryTest.mock_host_for_stub_repository()
        repository = StubRepository(cwd=host.getcwd(), filesystem=host)
        self.assertEqual(repository.native_revision(path=host.join(host.getcwd(), 'TestDirectory', 'TestDirectory2', 'TestDirectory3')), mock_stub_repository_json['id'])

    def test_native_branch(self):
        host = StubRepositoryTest.mock_host_for_stub_repository()
        repository = StubRepository(cwd=host.getcwd(), filesystem=host)
        self.assertEqual(repository.native_branch(path=host.join(host.getcwd(), 'TestDirectory', 'TestDirectory2', 'TestDirectory3')), mock_stub_repository_json['branch'])

    def test_svn_revision(self):
        host = StubRepositoryTest.mock_host_for_stub_repository()
        repository = StubRepository(cwd=host.getcwd(), filesystem=host)
        self.assertEqual(repository.svn_revision(path=host.join(host.getcwd(), 'TestDirectory', 'TestDirectory2', 'TestDirectory3')), mock_stub_repository_json['id'])

    def test_find_checkout_root(self):
        host = StubRepositoryTest.mock_host_for_stub_repository()
        repository = StubRepository(cwd=host.getcwd(), filesystem=host)
        self.assertEquals(repository.find_checkout_root(path=host.join('TestDirectory', 'TestDirectory2', 'TestDirectory3')), host.join(host.getcwd(), 'TestDirectory', 'TestDirectory2'))

    def test_find_checkout_root_failure(self):
        host = StubRepositoryTest.mock_host_for_stub_repository()
        repository = StubRepository(cwd=host.getcwd(), filesystem=host)
        self.assertIsNone(repository.find_checkout_root(path=host.getcwd()))

    def test_find_parent_path_matching_callback_condition_with_file_system(self):
        host = StubRepositoryTest.mock_host_for_stub_repository()
        self.assertIsNone(StubRepository._find_parent_path_matching_callback_condition(path=host.join('TestDirectory'), callback=lambda path: True, filesystem=host))

    def test_find_parent_path_matching_callback_condition_without_file_system(self):
        self.assertIsNone(StubRepository._find_parent_path_matching_callback_condition(path='/Volumes/', callback=lambda path: True, filesystem=None))
