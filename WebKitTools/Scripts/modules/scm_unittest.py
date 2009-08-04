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

import os
import subprocess
import tempfile
import unittest
from modules.scm import detect_scm_system, SCM, ScriptError


# Eventually we will want to write tests which work for both scms. (like update_webkit, changed_files, etc.)
# Perhaps through some SCMTest base-class which both SVNTest and GitTest inherit from.

def run(args):
    SCM.run_command(args)

# Exists to share svn repository creation code between the git and svn tests
class SVNTestRepository:
    @staticmethod
    def _setup_test_commits(test_object):
        # Add some test commits
        os.chdir(test_object.svn_checkout_path)
        test_file = open('test_file', 'w')
        test_file.write("test1")
        test_file.flush()
        
        run(['svn', 'add', 'test_file'])
        run(['svn', 'commit', '--quiet', '--message', 'initial commit'])
        
        test_file.write("test2")
        test_file.flush()
        
        run(['svn', 'commit', '--quiet', '--message', 'second commit'])
        
        test_file.write("test3")
        test_file.close()
        
        run(['svn', 'commit', '--quiet', '--message', 'third commit'])

    @classmethod
    def setup(cls, test_object):
        # Create an test SVN repository
        test_object.svn_repo_path = tempfile.mkdtemp(suffix="svn_test_repo")
        test_object.svn_repo_url = "file://%s" % test_object.svn_repo_path # Not sure this will work on windows
        # git svn complains if we don't pass --pre-1.5-compatible, not sure why:
        # Expected FS format '2'; found format '3' at /usr/local/libexec/git-core//git-svn line 1477
        run(['svnadmin', 'create', '--pre-1.5-compatible', test_object.svn_repo_path])

        # Create a test svn checkout
        test_object.svn_checkout_path = tempfile.mkdtemp(suffix="svn_test_checkout")
        run(['svn', 'checkout', '--quiet', test_object.svn_repo_url, test_object.svn_checkout_path])

        cls._setup_test_commits(test_object)

    @classmethod
    def tear_down(cls, test_object):
        run(['rm', '-rf', test_object.svn_repo_path])
        run(['rm', '-rf', test_object.svn_checkout_path])


class SVNTest(unittest.TestCase):

    def setUp(self):
        SVNTestRepository.setup(self)
        os.chdir(self.svn_checkout_path)

    def tearDown(self):
        SVNTestRepository.tear_down(self)

    def test_detection(self):
        scm = detect_scm_system(self.svn_checkout_path)
        self.assertEqual(scm.display_name(), "svn")
        self.assertEqual(scm.supports_local_commits(), False)

class GitTest(unittest.TestCase):

    def _setup_git_clone_of_svn_repository(self):
        self.git_checkout_path = tempfile.mkdtemp(suffix="git_test_checkout")
        # --quiet doesn't make git svn silent, so we redirect output
        args = ['git', 'svn', '--quiet', 'clone', self.svn_repo_url, self.git_checkout_path]
        git_svn_clone = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        git_svn_clone.communicate() # ignore output
        git_svn_clone.wait()

    def _tear_down_git_clone_of_svn_repository(self):
        run(['rm', '-rf', self.git_checkout_path])

    def setUp(self):
        SVNTestRepository.setup(self)
        self._setup_git_clone_of_svn_repository()
        os.chdir(self.git_checkout_path)

    def tearDown(self):
        SVNTestRepository.tear_down(self)
        self._tear_down_git_clone_of_svn_repository()

    def test_detection(self):
        scm = detect_scm_system(self.git_checkout_path)
        self.assertEqual(scm.display_name(), "git")
        self.assertEqual(scm.supports_local_commits(), True)

    def test_commitish_parsing(self):
        scm = detect_scm_system(self.git_checkout_path)

        # Multiple revisions are cherry-picked.
        self.assertEqual(len(scm.commit_ids_from_commitish_arguments(['HEAD~2'])), 1)
        self.assertEqual(len(scm.commit_ids_from_commitish_arguments(['HEAD', 'HEAD~2'])), 2)

        # ... is an invalid range specifier
        self.assertRaises(ScriptError, scm.commit_ids_from_commitish_arguments, ['trunk...HEAD'])


if __name__ == '__main__':
    unittest.main()
