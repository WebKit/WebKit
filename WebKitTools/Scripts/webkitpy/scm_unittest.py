# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Apple Inc. All rights reserved.
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

import base64
import os
import os.path
import re
import stat
import subprocess
import tempfile
import unittest
import urllib

from datetime import date
from webkitpy.executive import Executive, run_command, ScriptError
from webkitpy.scm import detect_scm_system, SCM, CheckoutNeedsUpdate, commit_error_handler
from webkitpy.bugzilla import Attachment # FIXME: This should not be needed

# Eventually we will want to write tests which work for both scms. (like update_webkit, changed_files, etc.)
# Perhaps through some SCMTest base-class which both SVNTest and GitTest inherit from.

# FIXME: This should be unified into one of the executive.py commands!
def run_silent(args, cwd=None):
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd)
    process.communicate() # ignore output
    exit_code = process.wait()
    if exit_code:
        raise ScriptError('Failed to run "%s"  exit_code: %d  cwd: %s' % (args, exit_code, cwd))

def write_into_file_at_path(file_path, contents):
    file = open(file_path, 'w')
    file.write(contents)
    file.close()

def read_from_path(file_path):
    file = open(file_path, 'r')
    contents = file.read()
    file.close()
    return contents

# Exists to share svn repository creation code between the git and svn tests
class SVNTestRepository:
    @staticmethod
    def _setup_test_commits(test_object):
        # Add some test commits
        os.chdir(test_object.svn_checkout_path)
        test_file = open('test_file', 'w')
        test_file.write("test1")
        test_file.flush()
        
        run_command(['svn', 'add', 'test_file'])
        run_command(['svn', 'commit', '--quiet', '--message', 'initial commit'])
        
        test_file.write("test2")
        test_file.flush()
        
        run_command(['svn', 'commit', '--quiet', '--message', 'second commit'])
        
        test_file.write("test3\n")
        test_file.flush()
        
        run_command(['svn', 'commit', '--quiet', '--message', 'third commit'])

        test_file.write("test4\n")
        test_file.close()

        run_command(['svn', 'commit', '--quiet', '--message', 'fourth commit'])

        # svn does not seem to update after commit as I would expect.
        run_command(['svn', 'update'])

    @classmethod
    def setup(cls, test_object):
        # Create an test SVN repository
        test_object.svn_repo_path = tempfile.mkdtemp(suffix="svn_test_repo")
        test_object.svn_repo_url = "file://%s" % test_object.svn_repo_path # Not sure this will work on windows
        # git svn complains if we don't pass --pre-1.5-compatible, not sure why:
        # Expected FS format '2'; found format '3' at /usr/local/libexec/git-core//git-svn line 1477
        run_command(['svnadmin', 'create', '--pre-1.5-compatible', test_object.svn_repo_path])

        # Create a test svn checkout
        test_object.svn_checkout_path = tempfile.mkdtemp(suffix="svn_test_checkout")
        run_command(['svn', 'checkout', '--quiet', test_object.svn_repo_url, test_object.svn_checkout_path])

        cls._setup_test_commits(test_object)

    @classmethod
    def tear_down(cls, test_object):
        run_command(['rm', '-rf', test_object.svn_repo_path])
        run_command(['rm', '-rf', test_object.svn_checkout_path])

# For testing the SCM baseclass directly.
class SCMClassTests(unittest.TestCase):
    def setUp(self):
        self.dev_null = open(os.devnull, "w") # Used to make our Popen calls quiet.

    def tearDown(self):
        self.dev_null.close()

    def test_run_command_with_pipe(self):
        input_process = subprocess.Popen(['echo', 'foo\nbar'], stdout=subprocess.PIPE, stderr=self.dev_null)
        self.assertEqual(run_command(['grep', 'bar'], input=input_process.stdout), "bar\n")

        # Test the non-pipe case too:
        self.assertEqual(run_command(['grep', 'bar'], input="foo\nbar"), "bar\n")

        command_returns_non_zero = ['/bin/sh', '--invalid-option']
        # Test when the input pipe process fails.
        input_process = subprocess.Popen(command_returns_non_zero, stdout=subprocess.PIPE, stderr=self.dev_null)
        self.assertTrue(input_process.poll() != 0)
        self.assertRaises(ScriptError, run_command, ['grep', 'bar'], input=input_process.stdout)

        # Test when the run_command process fails.
        input_process = subprocess.Popen(['echo', 'foo\nbar'], stdout=subprocess.PIPE, stderr=self.dev_null) # grep shows usage and calls exit(2) when called w/o arguments.
        self.assertRaises(ScriptError, run_command, command_returns_non_zero, input=input_process.stdout)

    def test_error_handlers(self):
        git_failure_message="Merge conflict during commit: Your file or directory 'WebCore/ChangeLog' is probably out-of-date: resource out of date; try updating at /usr/local/libexec/git-core//git-svn line 469"
        svn_failure_message="""svn: Commit failed (details follow):
svn: File or directory 'ChangeLog' is out of date; try updating
svn: resource out of date; try updating
"""
        command_does_not_exist = ['does_not_exist', 'invalid_option']
        self.assertRaises(OSError, run_command, command_does_not_exist)
        self.assertRaises(OSError, run_command, command_does_not_exist, error_handler=Executive.ignore_error)

        command_returns_non_zero = ['/bin/sh', '--invalid-option']
        self.assertRaises(ScriptError, run_command, command_returns_non_zero)
        # Check if returns error text:
        self.assertTrue(run_command(command_returns_non_zero, error_handler=Executive.ignore_error))

        self.assertRaises(CheckoutNeedsUpdate, commit_error_handler, ScriptError(output=git_failure_message))
        self.assertRaises(CheckoutNeedsUpdate, commit_error_handler, ScriptError(output=svn_failure_message))
        self.assertRaises(ScriptError, commit_error_handler, ScriptError(output='blah blah blah'))


# GitTest and SVNTest inherit from this so any test_ methods here will be run once for this class and then once for each subclass.
class SCMTest(unittest.TestCase):
    def _create_patch(self, patch_contents):
        patch_path = os.path.join(self.svn_checkout_path, 'patch.diff')
        write_into_file_at_path(patch_path, patch_contents)
        patch = {}
        patch['reviewer'] = 'Joe Cool'
        patch['bug_id'] = '12345'
        patch['url'] = 'file://%s' % urllib.pathname2url(patch_path)
        return Attachment(patch, None) # FIXME: This is a hack, scm.py shouldn't be fetching attachment data.

    def _setup_webkittools_scripts_symlink(self, local_scm):
        webkit_scm = detect_scm_system(os.path.dirname(os.path.abspath(__file__)))
        webkit_scripts_directory = webkit_scm.scripts_directory()
        local_scripts_directory = local_scm.scripts_directory()
        os.mkdir(os.path.dirname(local_scripts_directory))
        os.symlink(webkit_scripts_directory, local_scripts_directory)

    # Tests which both GitTest and SVNTest should run.
    # FIXME: There must be a simpler way to add these w/o adding a wrapper method to both subclasses
    def _shared_test_commit_with_message(self):
        write_into_file_at_path('test_file', 'more test content')
        commit_text = self.scm.commit_with_message('another test commit')
        self.assertEqual(self.scm.svn_revision_from_commit_text(commit_text), '5')

        self.scm.dryrun = True
        write_into_file_at_path('test_file', 'still more test content')
        commit_text = self.scm.commit_with_message('yet another test commit')
        self.assertEqual(self.scm.svn_revision_from_commit_text(commit_text), '0')

    def _shared_test_reverse_diff(self):
        self._setup_webkittools_scripts_symlink(self.scm) # Git's apply_reverse_diff uses resolve-ChangeLogs
        # Only test the simple case, as any other will end up with conflict markers.
        self.scm.apply_reverse_diff('4')
        self.assertEqual(read_from_path('test_file'), "test1test2test3\n")

    def _shared_test_diff_for_revision(self):
        # Patch formats are slightly different between svn and git, so just regexp for things we know should be there.
        r3_patch = self.scm.diff_for_revision(3)
        self.assertTrue(re.search('test3', r3_patch))
        self.assertFalse(re.search('test4', r3_patch))
        self.assertTrue(re.search('test2', r3_patch))
        self.assertTrue(re.search('test2', self.scm.diff_for_revision(2)))

    def _shared_test_svn_apply_git_patch(self):
        self._setup_webkittools_scripts_symlink(self.scm)
        git_binary_addition = """diff --git a/fizzbuzz7.gif b/fizzbuzz7.gif
new file mode 100644
index 0000000000000000000000000000000000000000..64a9532e7794fcd791f6f12157406d90
60151690
GIT binary patch
literal 512
zcmZ?wbhEHbRAx|MU|?iW{Kxc~?KofD;ckY;H+&5HnHl!!GQMD7h+sU{_)e9f^V3c?
zhJP##HdZC#4K}7F68@!1jfWQg2daCm-gs#3|JREDT>c+pG4L<_2;w##WMO#ysPPap
zLqpAf1OE938xAsSp4!5f-o><?VKe(#0jEcwfHGF4%M1^kRs14oVBp2ZEL{E1N<-zJ
zsfLmOtKta;2_;2c#^S1-8cf<nb!QnGl>c!Xe6RXvrEtAWBvSDTgTO1j3vA31Puw!A
zs(87q)j_mVDTqBo-P+03-P5mHCEnJ+x}YdCuS7#bCCyePUe(ynK+|4b-3qK)T?Z&)
zYG+`tl4h?GZv_$t82}X4*DTE|$;{DEiPyF@)U-1+FaX++T9H{&%cag`W1|zVP@`%b
zqiSkp6{BTpWTkCr!=<C6Q=?#~R8^JfrliAF6Q^gV9Iup8RqCXqqhqC`qsyhk<-nlB
z00f{QZvfK&|Nm#oZ0TQl`Yr$BIa6A@16O26ud7H<QM=xl`toLKnz-3h@9c9q&wm|X
z{89I|WPyD!*M?gv?q`;L=2YFeXrJQNti4?}s!zFo=5CzeBxC69xA<zrjP<wUcCRh4
ptUl-ZG<%a~#LwkIWv&q!KSCH7tQ8cJDiw+|GV?MN)RjY50RTb-xvT&H

literal 0
HcmV?d00001

"""
        self.scm.apply_patch(self._create_patch(git_binary_addition))
        added = read_from_path('fizzbuzz7.gif')
        self.assertEqual(512, len(added))
        self.assertTrue(added.startswith('GIF89a'))
        self.assertTrue('fizzbuzz7.gif' in self.scm.changed_files())

        # The file already exists.
        self.assertRaises(ScriptError, self.scm.apply_patch, self._create_patch(git_binary_addition))

        git_binary_modification = """diff --git a/fizzbuzz7.gif b/fizzbuzz7.gif
index 64a9532e7794fcd791f6f12157406d9060151690..323fae03f4606ea9991df8befbb2fca7
GIT binary patch
literal 7
OcmYex&reD$;sO8*F9L)B

literal 512
zcmZ?wbhEHbRAx|MU|?iW{Kxc~?KofD;ckY;H+&5HnHl!!GQMD7h+sU{_)e9f^V3c?
zhJP##HdZC#4K}7F68@!1jfWQg2daCm-gs#3|JREDT>c+pG4L<_2;w##WMO#ysPPap
zLqpAf1OE938xAsSp4!5f-o><?VKe(#0jEcwfHGF4%M1^kRs14oVBp2ZEL{E1N<-zJ
zsfLmOtKta;2_;2c#^S1-8cf<nb!QnGl>c!Xe6RXvrEtAWBvSDTgTO1j3vA31Puw!A
zs(87q)j_mVDTqBo-P+03-P5mHCEnJ+x}YdCuS7#bCCyePUe(ynK+|4b-3qK)T?Z&)
zYG+`tl4h?GZv_$t82}X4*DTE|$;{DEiPyF@)U-1+FaX++T9H{&%cag`W1|zVP@`%b
zqiSkp6{BTpWTkCr!=<C6Q=?#~R8^JfrliAF6Q^gV9Iup8RqCXqqhqC`qsyhk<-nlB
z00f{QZvfK&|Nm#oZ0TQl`Yr$BIa6A@16O26ud7H<QM=xl`toLKnz-3h@9c9q&wm|X
z{89I|WPyD!*M?gv?q`;L=2YFeXrJQNti4?}s!zFo=5CzeBxC69xA<zrjP<wUcCRh4
ptUl-ZG<%a~#LwkIWv&q!KSCH7tQ8cJDiw+|GV?MN)RjY50RTb-xvT&H

"""
        self.scm.apply_patch(self._create_patch(git_binary_modification))
        modified = read_from_path('fizzbuzz7.gif')
        self.assertEqual('foobar\n', modified)
        self.assertTrue('fizzbuzz7.gif' in self.scm.changed_files())

        # Applying the same modification should fail.
        self.assertRaises(ScriptError, self.scm.apply_patch, self._create_patch(git_binary_modification))

        git_binary_deletion = """diff --git a/fizzbuzz7.gif b/fizzbuzz7.gif
deleted file mode 100644
index 323fae0..0000000
GIT binary patch
literal 0
HcmV?d00001

literal 7
OcmYex&reD$;sO8*F9L)B

"""
        self.scm.apply_patch(self._create_patch(git_binary_deletion))
        self.assertFalse(os.path.exists('fizzbuzz7.gif'))
        self.assertFalse('fizzbuzz7.gif' in self.scm.changed_files())

        # Cannot delete again.
        self.assertRaises(ScriptError, self.scm.apply_patch, self._create_patch(git_binary_deletion))


class SVNTest(SCMTest):

    @staticmethod
    def _set_date_and_reviewer(changelog_entry):
        # Joe Cool matches the reviewer set in SCMTest._create_patch
        changelog_entry = changelog_entry.replace('REVIEWER_HERE', 'Joe Cool')
        # svn-apply will update ChangeLog entries with today's date.
        return changelog_entry.replace('DATE_HERE', date.today().isoformat())

    def test_svn_apply(self):
        first_entry = """2009-10-26  Eric Seidel  <eric@webkit.org>

        Reviewed by Foo Bar.

        Most awesome change ever.

        * scm_unittest.py:
"""
        intermediate_entry = """2009-10-27  Eric Seidel  <eric@webkit.org>

        Reviewed by Baz Bar.

        A more awesomer change yet!

        * scm_unittest.py:
"""
        one_line_overlap_patch = """Index: ChangeLog
===================================================================
--- ChangeLog	(revision 5)
+++ ChangeLog	(working copy)
@@ -1,5 +1,13 @@
 2009-10-26  Eric Seidel  <eric@webkit.org>

+        Reviewed by NOBODY (OOPS!).
+
+        Second most awsome change ever.
+
+        * scm_unittest.py:
+
+2009-10-26  Eric Seidel  <eric@webkit.org>
+
         Reviewed by Foo Bar.

         Most awesome change ever.
"""
        one_line_overlap_entry = """DATE_HERE  Eric Seidel  <eric@webkit.org>

        Reviewed by REVIEWER_HERE.

        Second most awsome change ever.

        * scm_unittest.py:
"""
        two_line_overlap_patch = """Index: ChangeLog
===================================================================
--- ChangeLog	(revision 5)
+++ ChangeLog	(working copy)
@@ -2,6 +2,14 @@

         Reviewed by Foo Bar.

+        Second most awsome change ever.
+
+        * scm_unittest.py:
+
+2009-10-26  Eric Seidel  <eric@webkit.org>
+
+        Reviewed by Foo Bar.
+
         Most awesome change ever.

         * scm_unittest.py:
"""
        two_line_overlap_entry = """DATE_HERE  Eric Seidel  <eric@webkit.org>

        Reviewed by Foo Bar.

        Second most awsome change ever.

        * scm_unittest.py:
"""
        write_into_file_at_path('ChangeLog', first_entry)
        run_command(['svn', 'add', 'ChangeLog'])
        run_command(['svn', 'commit', '--quiet', '--message', 'ChangeLog commit'])

        # Patch files were created against just 'first_entry'.
        # Add a second commit to make svn-apply have to apply the patches with fuzz.
        changelog_contents = "%s\n%s" % (intermediate_entry, first_entry)
        write_into_file_at_path('ChangeLog', changelog_contents)
        run_command(['svn', 'commit', '--quiet', '--message', 'Intermediate commit'])

        self._setup_webkittools_scripts_symlink(self.scm)
        self.scm.apply_patch(self._create_patch(one_line_overlap_patch))
        expected_changelog_contents = "%s\n%s" % (self._set_date_and_reviewer(one_line_overlap_entry), changelog_contents)
        self.assertEquals(read_from_path('ChangeLog'), expected_changelog_contents)

        self.scm.revert_files(['ChangeLog'])
        self.scm.apply_patch(self._create_patch(two_line_overlap_patch))
        expected_changelog_contents = "%s\n%s" % (self._set_date_and_reviewer(two_line_overlap_entry), changelog_contents)
        self.assertEquals(read_from_path('ChangeLog'), expected_changelog_contents)

    def setUp(self):
        SVNTestRepository.setup(self)
        os.chdir(self.svn_checkout_path)
        self.scm = detect_scm_system(self.svn_checkout_path)

    def tearDown(self):
        SVNTestRepository.tear_down(self)

    def test_create_patch_is_full_patch(self):
        test_dir_path = os.path.join(self.svn_checkout_path, 'test_dir')
        os.mkdir(test_dir_path)
        test_file_path = os.path.join(test_dir_path, 'test_file2')
        write_into_file_at_path(test_file_path, 'test content')
        run_command(['svn', 'add', 'test_dir'])

        # create_patch depends on 'svn-create-patch', so make a dummy version.
        scripts_path = os.path.join(self.svn_checkout_path, 'WebKitTools', 'Scripts')
        os.makedirs(scripts_path)
        create_patch_path = os.path.join(scripts_path, 'svn-create-patch')
        write_into_file_at_path(create_patch_path, '#!/bin/sh\necho $PWD') # We could pass -n to prevent the \n, but not all echo accept -n.
        os.chmod(create_patch_path, stat.S_IXUSR | stat.S_IRUSR)

        # Change into our test directory and run the create_patch command.
        os.chdir(test_dir_path)
        scm = detect_scm_system(test_dir_path)
        self.assertEqual(scm.checkout_root, self.svn_checkout_path) # Sanity check that detection worked right.
        patch_contents = scm.create_patch()
        # Our fake 'svn-create-patch' returns $PWD instead of a patch, check that it was executed from the root of the repo.
        self.assertEqual("%s\n" % os.path.realpath(scm.checkout_root), patch_contents) # Add a \n because echo adds a \n.

    def test_detection(self):
        scm = detect_scm_system(self.svn_checkout_path)
        self.assertEqual(scm.display_name(), "svn")
        self.assertEqual(scm.supports_local_commits(), False)

    def test_apply_small_binary_patch(self):
        patch_contents = """Index: test_file.swf
===================================================================
Cannot display: file marked as a binary type.
svn:mime-type = application/octet-stream

Property changes on: test_file.swf
___________________________________________________________________
Name: svn:mime-type
   + application/octet-stream


Q1dTBx0AAAB42itg4GlgYJjGwMDDyODMxMDw34GBgQEAJPQDJA==
"""
        expected_contents = base64.b64decode("Q1dTBx0AAAB42itg4GlgYJjGwMDDyODMxMDw34GBgQEAJPQDJA==")
        self._setup_webkittools_scripts_symlink(self.scm)
        patch_file = self._create_patch(patch_contents)
        self.scm.apply_patch(patch_file)
        actual_contents = read_from_path("test_file.swf")
        self.assertEqual(actual_contents, expected_contents)

    def test_apply_svn_patch(self):
        scm = detect_scm_system(self.svn_checkout_path)
        patch = self._create_patch(run_command(['svn', 'diff', '-r4:3']))
        self._setup_webkittools_scripts_symlink(scm)
        scm.apply_patch(patch)

    def test_apply_svn_patch_force(self):
        scm = detect_scm_system(self.svn_checkout_path)
        patch = self._create_patch(run_command(['svn', 'diff', '-r2:4']))
        self._setup_webkittools_scripts_symlink(scm)
        self.assertRaises(ScriptError, scm.apply_patch, patch, force=True)

    def test_commit_logs(self):
        # Commits have dates and usernames in them, so we can't just direct compare.
        self.assertTrue(re.search('fourth commit', self.scm.last_svn_commit_log()))
        self.assertTrue(re.search('second commit', self.scm.svn_commit_log(2)))

    def test_commit_text_parsing(self):
        self._shared_test_commit_with_message()

    def test_reverse_diff(self):
        self._shared_test_reverse_diff()

    def test_diff_for_revision(self):
        self._shared_test_diff_for_revision()

    def test_svn_apply_git_patch(self):
        self._shared_test_svn_apply_git_patch()

class GitTest(SCMTest):

    def _setup_git_clone_of_svn_repository(self):
        self.git_checkout_path = tempfile.mkdtemp(suffix="git_test_checkout")
        # --quiet doesn't make git svn silent, so we use run_silent to redirect output
        run_silent(['git', 'svn', '--quiet', 'clone', self.svn_repo_url, self.git_checkout_path])

    def _tear_down_git_clone_of_svn_repository(self):
        run_command(['rm', '-rf', self.git_checkout_path])

    def setUp(self):
        SVNTestRepository.setup(self)
        self._setup_git_clone_of_svn_repository()
        os.chdir(self.git_checkout_path)
        self.scm = detect_scm_system(self.git_checkout_path)

    def tearDown(self):
        SVNTestRepository.tear_down(self)
        self._tear_down_git_clone_of_svn_repository()

    def test_detection(self):
        scm = detect_scm_system(self.git_checkout_path)
        self.assertEqual(scm.display_name(), "git")
        self.assertEqual(scm.supports_local_commits(), True)

    def test_rebase_in_progress(self):
        svn_test_file = os.path.join(self.svn_checkout_path, 'test_file')
        write_into_file_at_path(svn_test_file, "svn_checkout")
        run_command(['svn', 'commit', '--message', 'commit to conflict with git commit'], cwd=self.svn_checkout_path)

        git_test_file = os.path.join(self.git_checkout_path, 'test_file')
        write_into_file_at_path(git_test_file, "git_checkout")
        run_command(['git', 'commit', '-a', '-m', 'commit to be thrown away by rebase abort'])

        # --quiet doesn't make git svn silent, so use run_silent to redirect output
        self.assertRaises(ScriptError, run_silent, ['git', 'svn', '--quiet', 'rebase']) # Will fail due to a conflict leaving us mid-rebase.

        scm = detect_scm_system(self.git_checkout_path)
        self.assertTrue(scm.rebase_in_progress())

        # Make sure our cleanup works.
        scm.clean_working_directory()
        self.assertFalse(scm.rebase_in_progress())

        # Make sure cleanup doesn't throw when no rebase is in progress.
        scm.clean_working_directory()

    def test_commitish_parsing(self):
        scm = detect_scm_system(self.git_checkout_path)
    
        # Multiple revisions are cherry-picked.
        self.assertEqual(len(scm.commit_ids_from_commitish_arguments(['HEAD~2'])), 1)
        self.assertEqual(len(scm.commit_ids_from_commitish_arguments(['HEAD', 'HEAD~2'])), 2)
    
        # ... is an invalid range specifier
        self.assertRaises(ScriptError, scm.commit_ids_from_commitish_arguments, ['trunk...HEAD'])

    def test_commitish_order(self):
        scm = detect_scm_system(self.git_checkout_path)

        commit_range = 'HEAD~3..HEAD'

        actual_commits = scm.commit_ids_from_commitish_arguments([commit_range])
        expected_commits = []
        expected_commits += reversed(run_command(['git', 'rev-list', commit_range]).splitlines())

        self.assertEqual(actual_commits, expected_commits)

    def test_apply_git_patch(self):
        scm = detect_scm_system(self.git_checkout_path)
        patch = self._create_patch(run_command(['git', 'diff', 'HEAD..HEAD^']))
        self._setup_webkittools_scripts_symlink(scm)
        scm.apply_patch(patch)

    def test_apply_git_patch_force(self):
        scm = detect_scm_system(self.git_checkout_path)
        patch = self._create_patch(run_command(['git', 'diff', 'HEAD~2..HEAD']))
        self._setup_webkittools_scripts_symlink(scm)
        self.assertRaises(ScriptError, scm.apply_patch, patch, force=True)

    def test_commit_text_parsing(self):
        self._shared_test_commit_with_message()

    def test_reverse_diff(self):
        self._shared_test_reverse_diff()

    def test_diff_for_revision(self):
        self._shared_test_diff_for_revision()

    def test_svn_apply_git_patch(self):
        self._shared_test_svn_apply_git_patch()

    def test_create_binary_patch(self):
        # Create a git binary patch and check the contents.
        scm = detect_scm_system(self.git_checkout_path)
        test_file_name = 'binary_file'
        test_file_path = os.path.join(self.git_checkout_path, test_file_name)
        file_contents = ''.join(map(chr, range(256)))
        write_into_file_at_path(test_file_path, file_contents)
        run_command(['git', 'add', test_file_name])
        patch = scm.create_patch()
        self.assertTrue(re.search(r'\nliteral 0\n', patch))
        self.assertTrue(re.search(r'\nliteral 256\n', patch))

        # Check if we can apply the created patch.
        run_command(['git', 'rm', '-f', test_file_name])
        self._setup_webkittools_scripts_symlink(scm)
        self.scm.apply_patch(self._create_patch(patch))
        self.assertEqual(file_contents, read_from_path(test_file_path))

        # Check if we can create a patch from a local commit.
        write_into_file_at_path(test_file_path, file_contents)
        run_command(['git', 'add', test_file_name])
        run_command(['git', 'commit', '-m', 'binary diff'])
        patch_from_local_commit = scm.create_patch_from_local_commit('HEAD')
        self.assertTrue(re.search(r'\nliteral 0\n', patch_from_local_commit))
        self.assertTrue(re.search(r'\nliteral 256\n', patch_from_local_commit))
        patch_since_local_commit = scm.create_patch_since_local_commit('HEAD^1')
        self.assertTrue(re.search(r'\nliteral 0\n', patch_since_local_commit))
        self.assertTrue(re.search(r'\nliteral 256\n', patch_since_local_commit))
        self.assertEqual(patch_from_local_commit, patch_since_local_commit)


if __name__ == '__main__':
    unittest.main()
