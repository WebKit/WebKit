# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009, 2016, 2020 Apple Inc. All rights reserved.
# Copyright (C) 2011 Daniel Bates (dbates@intudata.com). All rights reserved.
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

import atexit
import base64
import codecs
import getpass
import logging
import os
import os.path
import re
import stat
import sys
import subprocess
import tempfile
import time
import urllib
import shutil
import unittest

from datetime import date
from webkitcorepy import Version

from webkitpy.common.checkout.checkout import Checkout
from webkitpy.common.config.committers import Committer  # FIXME: This should not be needed
from webkitpy.common.net.bugzilla import Attachment  # FIXME: This should not be needed
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.checkout.scm.git import Git, AmbiguousCommitError
from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.checkout.scm.scm import CheckoutNeedsUpdate, commit_error_handler, AuthenticationError

from webkitpy.test.markers import slow, xfail

from webkitcorepy import OutputCapture

try:
    import pathlib
except ImportError:
    import pathlib2 as pathlib


# We cache the mock SVN repo so that we don't create it again for each call to an SVNTest or GitTest test_ method.
# We store it in a global variable so that we can delete this cached repo on exit(3).
# FIXME: Remove this once test-webkitpy supports class and module fixtures (i.e. setUpClass()/setUpModule()
# are called exactly once per class/module).
cached_svn_repo_path = None


def remove_dir(path):
    # Change directory to / to ensure that we aren't in the directory we want to delete.
    os.chdir('/')
    shutil.rmtree(path)


# FIXME: Remove this once test-webkitpy supports class and module fixtures (i.e. setUpClass()/setUpModule()
# are called exactly once per class/module).
@atexit.register
def delete_cached_mock_repo_at_exit():
    if cached_svn_repo_path:
        remove_dir(cached_svn_repo_path)

# Eventually we will want to write tests which work for both scms. (like update_webkit, changed_files, etc.)
# Perhaps through some SCMTest base-class which both SVNTest and GitTest inherit from.


def run_command(*args, **kwargs):
    # FIXME: This should not be a global static.
    # New code should use Executive.run_command directly instead
    return Executive().run_command(*args, **kwargs)


# FIXME: This should be unified into one of the executive.py commands!
# Callers could use run_and_throw_if_fail(args, cwd=cwd, quiet=True)
def run_silent(args, cwd=None):
    # Note: Not thread safe: http://bugs.python.org/issue2320
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd)
    process.communicate()  # ignore output
    exit_code = process.wait()
    if exit_code:
        raise ScriptError('Failed to run "%s"  exit_code: %d  cwd: %s' % (args, exit_code, cwd))


def write_into_file_at_path(file_path, contents, encoding="utf-8"):
    if encoding:
        with codecs.open(file_path, "w", encoding) as file:
            return file.write(contents)
    with open(file_path, "wb") as file:
        return file.write(contents)


def read_from_path(file_path, encoding="utf-8"):
    if not encoding:
        with codecs.open(file_path, "rb") as file:
            return file.read()

    with codecs.open(file_path, "r", encoding) as file:
        return file.read()


def _make_diff(command, *args):
    # We use this wrapper to disable output decoding. diffs should be treated as
    # binary files since they may include text files of multiple differnet encodings.
    # FIXME: This should use an Executive.
    return run_command([command, "diff"] + list(args), decode_output=False)


def _svn_diff(*args):
    return _make_diff("svn", *args)


def _git_diff(*args):
    return _make_diff("git", *args)


# For testing the SCM baseclass directly.
class SCMClassTests(unittest.TestCase):
    def setUp(self):
        self.dev_null = open(os.devnull, "w")  # Used to make our Popen calls quiet.

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
        self.assertNotEqual(input_process.poll(), 0)
        self.assertRaises(ScriptError, run_command, ['grep', 'bar'], input=input_process.stdout)

        # Test when the run_command process fails.
        input_process = subprocess.Popen(['echo', 'foo\nbar'], stdout=subprocess.PIPE, stderr=self.dev_null)  # grep shows usage and calls exit(2) when called w/o arguments.
        self.assertRaises(ScriptError, run_command, command_returns_non_zero, input=input_process.stdout)

    def test_error_handlers(self):
        git_failure_message = "Merge conflict during commit: Your file or directory 'WebCore/ChangeLog' is probably out-of-date: resource out of date; try updating at /usr/local/libexec/git-core//git-svn line 469"
        svn_failure_message = """svn: Commit failed (details follow):
svn: File or directory 'ChangeLog' is out of date; try updating
svn: resource out of date; try updating
"""
        command_does_not_exist = ['does_not_exist', 'invalid_option']
        self.assertRaises(OSError, run_command, command_does_not_exist)
        self.assertRaises(OSError, run_command, command_does_not_exist, ignore_errors=True)

        command_returns_non_zero = ['/bin/sh', '--invalid-option']
        self.assertRaises(ScriptError, run_command, command_returns_non_zero)
        # Check if returns error text:
        self.assertTrue(run_command(command_returns_non_zero, ignore_errors=True))

        self.assertRaises(CheckoutNeedsUpdate, commit_error_handler, ScriptError(output=git_failure_message))
        self.assertRaises(CheckoutNeedsUpdate, commit_error_handler, ScriptError(output=svn_failure_message))
        self.assertRaises(ScriptError, commit_error_handler, ScriptError(output='blah blah blah'))


# GitTest and SVNTest inherit from this so any test_ methods here will be run once for this class and then once for each subclass.
class SCMTest(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(SCMTest, self).__init__(*args, **kwargs)
        self.scm = None

    def assertRegex(self, *args, **kwargs):
        try:
            return super(SCMTest, self).assertRegex(*args, **kwargs)
        except AttributeError:
            # Python 2
            return self.assertRegexpMatches(*args, **kwargs)

    def assertNotRegex(self, *args, **kwargs):
        try:
            return super(SCMTest, self).assertNotRegex(*args, **kwargs)
        except AttributeError:
            # Python 2
            return self.assertNotRegexpMatches(*args, **kwargs)

    def _create_patch(self, patch_contents):
        # FIXME: This code is brittle if the Attachment API changes.
        attachment = Attachment({"bug_id": 12345}, None)
        attachment.contents = lambda: patch_contents

        joe_cool = Committer("Joe Cool", "joe@cool.com")
        attachment.reviewer = lambda: joe_cool

        return attachment

    def _setup_webkittools_scripts_symlink(self, local_scm):
        webkit_scm = detect_scm_system(os.path.dirname(os.path.abspath(__file__)))
        webkit_scripts_directory = Checkout(webkit_scm, webkit_scm._executive, webkit_scm._filesystem).scripts_directory()
        local_scripts_directory = Checkout(local_scm, local_scm._executive, local_scm._filesystem).scripts_directory()
        os.mkdir(os.path.dirname(local_scripts_directory))
        os.symlink(webkit_scripts_directory, local_scripts_directory)

    # Tests which both GitTest and SVNTest should run.
    # FIXME: There must be a simpler way to add these w/o adding a wrapper method to both subclasses

    def _shared_test_changed_files(self):
        write_into_file_at_path("test_file", "changed content")
        self.assertEqual(self.scm.changed_files(), ["test_file"])
        write_into_file_at_path("test_dir/test_file3", "new stuff")
        self.assertEqual(sorted(self.scm.changed_files()), sorted(["test_dir/test_file3", "test_file"]))
        old_cwd = os.getcwd()
        os.chdir("test_dir")
        # Validate that changed_files does not change with our cwd, see bug 37015.
        self.assertEqual(sorted(self.scm.changed_files()), sorted(["test_dir/test_file3", "test_file"]))
        os.chdir(old_cwd)

    def _shared_test_untracked_files(self, scm):
        write_into_file_at_path("test_file_new", "new content")
        self.assertEqual(scm.untracked_files(), ["test_file_new"])

        os.mkdir("test_dir_new")
        write_into_file_at_path(os.path.join("test_dir_new", "test_file_new"), "new stuff")
        self.assertEqual(sorted(scm.untracked_files()), sorted(["test_dir_new", "test_file_new"]))

        old_cwd = os.getcwd()
        os.chdir("test_dir_new")
        # Validate that untracked_files do not change with our cwd.
        self.assertEqual(scm.untracked_files(), ["test_dir_new", "test_file_new"])

        os.chdir(old_cwd)

        write_into_file_at_path("test_file_new.pyc", "new ignored file")
        self.assertEqual(sorted(scm.untracked_files()), sorted(["test_dir_new", "test_file_new"]))
        self.assertEqual(sorted(scm.untracked_files(include_ignored_files=True)), sorted(["test_file_new.pyc", "test_dir_new", "test_file_new"]))

        shutil.rmtree("test_dir_new")
        os.remove("test_file_new")
        os.remove("test_file_new.pyc")

    def _shared_test_discard_untracked_files(self, scm):
        write_into_file_at_path("test_file_new", "new content")
        os.mkdir("test_dir_new")
        write_into_file_at_path("test_dir_new/test_file_new", "new stuff")
        self.assertEqual(sorted(scm.untracked_files()), sorted(["test_dir_new", "test_file_new"]))
        scm.discard_untracked_files()
        self.assertEqual(scm.untracked_files(), [])

        write_into_file_at_path("test_file_new.pyc", "new content")
        self.assertEqual(scm.untracked_files(True), ["test_file_new.pyc"])
        scm.discard_untracked_files(discard_ignored_files=False)
        self.assertEqual(scm.untracked_files(True), ["test_file_new.pyc"])
        scm.discard_untracked_files(discard_ignored_files=True)
        self.assertEqual(scm.untracked_files(True), [])

        os.mkdir("WebKitBuild")
        self.assertEqual(scm.untracked_files(True), ["WebKitBuild"])
        scm.discard_untracked_files(discard_ignored_files=True, keep_webkitbuild_directory=True)
        self.assertEqual(scm.untracked_files(True), ["WebKitBuild"])
        scm.discard_untracked_files(discard_ignored_files=True, keep_webkitbuild_directory=False)
        self.assertEqual(scm.untracked_files(True), [])

        if os.path.isdir("test_dir_new"):
            shutil.rmtree("test_dir_new")
        if os.path.isfile("test_file_new"):
            os.remove("test_file_new")
        if os.path.isfile("test_file_new.pyc"):
            os.remove("test_file_new.pyc")

    def _shared_test_added_files(self):
        write_into_file_at_path("test_file", "changed content")
        self.assertEqual(self.scm.added_files(), [])

        write_into_file_at_path("added_file", "new stuff")
        self.scm.add("added_file")

        write_into_file_at_path("added_file3", "more new stuff")
        write_into_file_at_path("added_file4", "more new stuff")
        self.scm.add_list(["added_file3", "added_file4"])

        os.mkdir("added_dir")
        write_into_file_at_path("added_dir/added_file2", "new stuff")
        self.scm.add("added_dir")

        # SVN reports directory changes, Git does not.
        added_files = self.scm.added_files()
        if "added_dir" in added_files:
            added_files.remove("added_dir")
        self.assertEqual(sorted(added_files), sorted(["added_dir/added_file2", "added_file", "added_file3", "added_file4"]))

        # Test also to make sure discard_working_directory_changes removes added files
        self.scm.discard_working_directory_changes()
        self.assertEqual(self.scm.added_files(), [])
        self.assertFalse(os.path.exists("added_file"))
        self.assertFalse(os.path.exists("added_file3"))
        self.assertFalse(os.path.exists("added_file4"))
        self.assertFalse(os.path.exists("added_dir"))

    def _shared_test_changed_files_for_revision(self):
        # SVN reports directory changes, Git does not.
        changed_files = self.scm.changed_files_for_revision(3)
        if "test_dir" in changed_files:
            changed_files.remove("test_dir")
        self.assertEqual(sorted(changed_files), sorted(["test_dir/test_file3", "test_file"]))
        self.assertEqual(sorted(self.scm.changed_files_for_revision(4)), sorted(["test_file", "test_file2"]))  # Git and SVN return different orders.
        self.assertEqual(self.scm.changed_files_for_revision(2), ["test_file"])

    def _shared_test_contents_at_revision(self):
        self.assertEqual(self.scm.contents_at_revision("test_file", 3), b"test1test2")
        self.assertEqual(self.scm.contents_at_revision("test_file", 4), b"test1test2test3\n")

        # Verify that contents_at_revision returns a byte array, aka str():
        self.assertEqual(self.scm.contents_at_revision("test_file", 5), u"latin1 test: \u00A0\n".encode("latin1"))
        self.assertEqual(self.scm.contents_at_revision("test_file2", 5), u"utf-8 test: \u00A0\n".encode("utf-8"))

        self.assertEqual(self.scm.contents_at_revision("test_file2", 4), b"second file")
        # Files which don't exist:
        # Currently we raise instead of returning None because detecting the difference between
        # "file not found" and any other error seems impossible with svn (git seems to expose such through the return code).
        self.assertRaises(ScriptError, self.scm.contents_at_revision, b"test_file2", 2)
        self.assertRaises(ScriptError, self.scm.contents_at_revision, b"does_not_exist", 2)

    def _shared_test_revisions_changing_file(self):
        self.assertEqual(sorted(self.scm.revisions_changing_file("test_file")), sorted([5, 4, 3, 2]))
        self.assertEqual(self.scm.revisions_changing_file("non_existent_file"), [])

    def _shared_test_committer_email_for_revision(self):
        self.assertEqual(self.scm.committer_email_for_revision(3), getpass.getuser())  # Committer "email" will be the current user

    def _shared_test_reverse_diff(self):
        self._setup_webkittools_scripts_symlink(self.scm)  # Git's apply_reverse_diff uses resolve-ChangeLogs.
        # Only test the simple case, as any other will end up with conflict markers.
        self.scm.apply_reverse_diff('5')
        self.assertEqual(read_from_path('test_file'), "test1test2test3\n")

    def _shared_test_add_recursively(self):
        os.mkdir("added_dir")
        write_into_file_at_path("added_dir/added_file", "new stuff")
        self.scm.add("added_dir/added_file")
        self.assertIn("added_dir/added_file", self.scm.added_files())

    def _shared_test_delete_recursively(self):
        os.mkdir("added_dir")
        write_into_file_at_path("added_dir/added_file", "new stuff")
        self.scm.add("added_dir/added_file")
        self.assertIn("added_dir/added_file", self.scm.added_files())
        self.scm.delete("added_dir/added_file")
        self.assertNotIn("added_dir", self.scm.added_files())

    def _shared_test_delete_recursively_or_not(self):
        os.mkdir("added_dir")
        write_into_file_at_path("added_dir/added_file", "new stuff")
        write_into_file_at_path("added_dir/another_added_file", "more new stuff")
        self.scm.add("added_dir/added_file")
        self.scm.add("added_dir/another_added_file")
        self.assertIn("added_dir/added_file", self.scm.added_files())
        self.assertIn("added_dir/another_added_file", self.scm.added_files())
        self.scm.delete("added_dir/added_file")
        self.assertIn("added_dir/another_added_file", self.scm.added_files())

    def _shared_test_exists(self, scm, commit_function):
        os.chdir(scm.checkout_root)
        self.assertFalse(scm.exists('foo.txt'))
        write_into_file_at_path('foo.txt', 'some stuff')
        self.assertFalse(scm.exists('foo.txt'))
        scm.add('foo.txt')
        commit_function('adding foo')
        self.assertTrue(scm.exists('foo.txt'))
        scm.delete('foo.txt')
        commit_function('deleting foo')
        self.assertFalse(scm.exists('foo.txt'))

    def _shared_test_svn_revision(self, scm):
        self.assertEqual(scm.svn_revision(scm.checkout_root), '5')

    def _shared_test_svn_branch(self, scm):
        self.assertEqual(scm.svn_branch(scm.checkout_root), 'trunk')

# Context manager that overrides the current timezone.
class TimezoneOverride(object):
    def __init__(self, timezone_string):
        self._timezone_string = timezone_string

    def __enter__(self):
        if hasattr(time, 'tzset'):
            self._saved_timezone = os.environ.get('TZ', None)
            os.environ['TZ'] = self._timezone_string
            time.tzset()

    def __exit__(self, type, value, traceback):
        if hasattr(time, 'tzset'):
            if self._saved_timezone:
                os.environ['TZ'] = self._saved_timezone
            else:
                del os.environ['TZ']
            time.tzset()


class GitTest(SCMTest):

    def setUp(self):
        """Sets up fresh git repository with one commit. Then setups a second git
        repo that tracks the first one."""
        # FIXME: We should instead clone a git repo that is tracking an SVN repo.
        # That better matches what we do with WebKit.
        self.original_dir = os.getcwd()

        self.untracking_checkout_path = tempfile.mkdtemp(suffix="git_test_checkout2")
        run_command(['git', 'init', self.untracking_checkout_path])

        os.chdir(self.untracking_checkout_path)
        run_command(['git', 'config', 'commit.gpgsign', 'false'])
        run_command(['git', 'config', 'user.name', 'scm_unittest'])
        run_command(['git', 'config', 'user.email', 'scm_unittest@example.com'])
        write_into_file_at_path('foo_file', 'foo')
        run_command(['git', 'add', 'foo_file'])
        write_into_file_at_path('.gitignore', '*.pyc')
        run_command(['git', 'add', '.gitignore'])
        run_command(['git', 'commit', '-am', 'dummy commit'])
        self.untracking_scm = detect_scm_system(self.untracking_checkout_path)

        self.tracking_git_checkout_path = tempfile.mkdtemp(suffix="git_test_checkout")
        run_command(['git', 'clone', '--quiet', self.untracking_checkout_path, self.tracking_git_checkout_path])
        os.chdir(self.tracking_git_checkout_path)
        run_command(['git', 'config', 'commit.gpgsign', 'false'])
        run_command(['git', 'config', 'user.name', 'scm_unittest'])
        run_command(['git', 'config', 'user.email', 'scm_unittest@example.com'])
        self.tracking_scm = detect_scm_system(self.tracking_git_checkout_path)

    def tearDown(self):
        # Change back to a valid directory so that later calls to os.getcwd() do not fail.
        os.chdir(self.original_dir)
        run_command(['rm', '-rf', self.tracking_git_checkout_path])
        run_command(['rm', '-rf', self.untracking_checkout_path])

    def test_remote_branch_ref(self):
        self.assertIn(self.tracking_scm.remote_branch_ref(), ('refs/remotes/origin/master', 'refs/remotes/origin/main'))

        os.chdir(self.untracking_checkout_path)
        self.assertRaises(ScriptError, self.untracking_scm.remote_branch_ref)

    @xfail
    def test_multiple_remotes(self):
        run_command(['git', 'config', '--add', 'svn-remote.svn.fetch', 'trunk:remote1'])
        run_command(['git', 'config', '--add', 'svn-remote.svn.fetch', 'trunk:remote2'])
        self.assertEqual(self.tracking_scm.remote_branch_ref(), 'remote1')

    def test_create_patch(self):
        write_into_file_at_path('test_file_commit1', 'contents')
        run_command(['git', 'add', 'test_file_commit1'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('message')

        patch = scm.create_patch()
        self.assertNotRegex(patch, b'Subversion Revision:')

    @xfail
    def test_create_patch_with_git_index(self):
        # First change. Committed.
        write_into_file_at_path('test_file_commit1', 'first cat')
        run_command(['git', 'add', 'test_file_commit1'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('message')

        # Second change. Staged but not committed.
        write_into_file_at_path('test_file_commit1', 'second dog')
        run_command(['git', 'add', 'test_file_commit1'])

        # Third change. Not even staged.
        write_into_file_at_path('test_file_commit1', 'third unicorn')

        patch = scm.create_patch(None, None, True)
        self.assertRegex(patch, b'-first cat')
        self.assertRegex(patch, b'\\+second dog')
        self.assertNotRegex(patch, b'third')
        self.assertNotRegex(patch, b'unicorn')

    def test_create_patch_merge_base_without_commit_message(self):
        write_into_file_at_path('foo_file', 'bar')
        run_command(['git', 'add', 'foo_file'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('a new commit')

        patch = scm.create_patch(commit_message=False)
        lines = patch.splitlines()
        self.assertNotIn(b'+foo', lines)
        self.assertIn(b'-foo', lines)
        self.assertIn(b'+bar', lines)

    def test_create_patch_merge_base_with_commit_message(self):
        write_into_file_at_path('foo_file', 'bar')
        run_command(['git', 'add', 'foo_file'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('a new commit')

        patch = scm.create_patch(commit_message=True)
        lines = patch.splitlines()
        self.assertNotIn(b'+foo', lines)
        self.assertIn(b'-foo', lines)
        self.assertIn(b'+bar', lines)

    def test_orderfile(self):
        os.mkdir("Tools")
        os.mkdir("Source")
        os.mkdir("LayoutTests")
        os.mkdir("Websites")

        # Slash should always be the right path separator since we use cygwin on Windows.
        Tools_ChangeLog = "Tools/ChangeLog"
        write_into_file_at_path(Tools_ChangeLog, "contents")
        Source_ChangeLog = "Source/ChangeLog"
        write_into_file_at_path(Source_ChangeLog, "contents")
        LayoutTests_ChangeLog = "LayoutTests/ChangeLog"
        write_into_file_at_path(LayoutTests_ChangeLog, "contents")
        Websites_ChangeLog = "Websites/ChangeLog"
        write_into_file_at_path(Websites_ChangeLog, "contents")

        Tools_ChangeFile = "Tools/ChangeFile"
        write_into_file_at_path(Tools_ChangeFile, "contents")
        Source_ChangeFile = "Source/ChangeFile"
        write_into_file_at_path(Source_ChangeFile, "contents")
        LayoutTests_ChangeFile = "LayoutTests/ChangeFile"
        write_into_file_at_path(LayoutTests_ChangeFile, "contents")
        Websites_ChangeFile = "Websites/ChangeFile"
        write_into_file_at_path(Websites_ChangeFile, "contents")

        run_command(['git', 'add', 'Tools/ChangeLog'])
        run_command(['git', 'add', 'LayoutTests/ChangeLog'])
        run_command(['git', 'add', 'Source/ChangeLog'])
        run_command(['git', 'add', 'Websites/ChangeLog'])
        run_command(['git', 'add', 'Tools/ChangeFile'])
        run_command(['git', 'add', 'LayoutTests/ChangeFile'])
        run_command(['git', 'add', 'Source/ChangeFile'])
        run_command(['git', 'add', 'Websites/ChangeFile'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('message')

        patch = scm.create_patch()
        self.assertTrue(re.search(b'Tools/ChangeLog', patch).start() < re.search(b'Tools/ChangeFile', patch).start())
        self.assertTrue(re.search(b'Websites/ChangeLog', patch).start() < re.search(b'Websites/ChangeFile', patch).start())
        self.assertTrue(re.search(b'Source/ChangeLog', patch).start() < re.search(b'Source/ChangeFile', patch).start())
        self.assertTrue(re.search(b'LayoutTests/ChangeLog', patch).start() < re.search(b'LayoutTests/ChangeFile', patch).start())

        self.assertTrue(re.search(b'Source/ChangeLog', patch).start() < re.search(b'LayoutTests/ChangeLog', patch).start())
        self.assertTrue(re.search(b'Tools/ChangeLog', patch).start() < re.search(b'LayoutTests/ChangeLog', patch).start())
        self.assertTrue(re.search(b'Websites/ChangeLog', patch).start() < re.search(b'LayoutTests/ChangeLog', patch).start())

        self.assertTrue(re.search(b'Source/ChangeFile', patch).start() < re.search(b'LayoutTests/ChangeLog', patch).start())
        self.assertTrue(re.search(b'Tools/ChangeFile', patch).start() < re.search(b'LayoutTests/ChangeLog', patch).start())
        self.assertTrue(re.search(b'Websites/ChangeFile', patch).start() < re.search(b'LayoutTests/ChangeLog', patch).start())

        self.assertTrue(re.search(b'Source/ChangeFile', patch).start() < re.search(b'LayoutTests/ChangeFile', patch).start())
        self.assertTrue(re.search(b'Tools/ChangeFile', patch).start() < re.search(b'LayoutTests/ChangeFile', patch).start())
        self.assertTrue(re.search(b'Websites/ChangeFile', patch).start() < re.search(b'LayoutTests/ChangeFile', patch).start())

    def test_exists(self):
        scm = self.untracking_scm
        self._shared_test_exists(scm, scm.commit_locally_with_message)

    def test_native_revision(self):
        scm = self.tracking_scm
        command = ['git', '-C', scm.checkout_root, 'rev-parse', 'HEAD']
        self.assertEqual(scm.native_revision(scm.checkout_root), run_command(command).strip())

    def test_native_branch(self):
        scm = self.tracking_scm
        self.assertIn(scm.native_branch(scm.checkout_root), ('master', 'main'))

    @xfail
    def test_rename_files(self):
        scm = self.tracking_scm

        run_command(['git', 'mv', 'foo_file', 'bar_file'])
        scm.commit_locally_with_message('message')

        patch = scm.create_patch()
        self.assertNotRegex(patch, b'rename from ')
        self.assertNotRegex(patch, b'rename to ')

    def test_untracked_files(self):
        self._shared_test_untracked_files(self.tracking_scm)


# We need to split off more of these SCM tests to use mocks instead of the filesystem.
# This class is the first part of that.
class GitTestWithMock(unittest.TestCase):
    maxDiff = None

    def make_scm(self, logging_executive=False):
        # We do this should_log dance to avoid logging when Git.__init__ runs sysctl on mac to check for 64-bit support.
        scm = Git(cwd=".", patch_directories=None, executive=MockExecutive(), filesystem=MockFileSystem())
        scm.read_git_config = lambda *args, **kw: "MOCKKEY:MOCKVALUE"
        scm._executive._should_log = logging_executive
        return scm

    @xfail
    def test_create_patch(self):
        scm = self.make_scm(logging_executive=True)
        with OutputCapture(level=logging.INFO) as captured:
            scm.create_patch()
        self.assertEqual(
            captured.stderr.getvalue(),
            '''MOCK run_command: ['git', 'merge-base', 'MOCKVALUE', 'HEAD'], cwd={checkout}s
MOCK run_command: ['git', 'diff', '--binary', '--no-color', '--no-ext-diff', '--full-index', '--no-renames', '', 'MOCK output of child process', '--'], cwd=%(checkout)s
MOCK run_command: ['git', 'rev-parse', '--show-toplevel'], cwd={checkout}s
MOCK run_command: ['git', 'log', '-1', '--grep=git-svn-id:', '--date=iso', './MOCK output of child process/MOCK output of child process'], cwd=%(checkout)s
'''.format(checkout=scm.checkout_root),
        )

    def test_push_local_commits_to_server_with_username_and_password(self):
        self.assertEqual(self.make_scm().push_local_commits_to_server(username='dbates@webkit.org', password='blah'), "MOCK output of child process")

    def test_push_local_commits_to_server_without_username_and_password(self):
        self.assertRaises(AuthenticationError, self.make_scm().push_local_commits_to_server)

    def test_push_local_commits_to_server_with_username_and_without_password(self):
        self.assertRaises(AuthenticationError, self.make_scm().push_local_commits_to_server, {'username': 'dbates@webkit.org'})

    def test_push_local_commits_to_server_without_username_and_with_password(self):
        self.assertRaises(AuthenticationError, self.make_scm().push_local_commits_to_server, {'password': 'blah'})

    def test_timestamp_of_revision(self):
        scm = self.make_scm()
        scm.find_checkout_root = lambda path: ''
        scm._run_git = lambda args: 'Date: 2013-02-08 08:05:49 +0000'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-08T08:05:49Z')

        scm._run_git = lambda args: 'Date: 2013-02-08 01:02:03 +0130'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-07T23:32:03Z')

        scm._run_git = lambda args: 'Date: 2013-02-08 01:55:21 -0800'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-08T09:55:21Z')

    def test_timestamp_of_native_revision(self):
        scm = self.make_scm()
        scm.find_checkout_root = lambda path: ''
        scm._run_git = lambda args: '1360310749'
        self.assertEqual(scm.timestamp_of_native_revision('some-path', '1a1c3b08814bc2a8c15b0f6db63cdce68f2aaa7a'), '2013-02-08T08:05:49Z')

        scm._run_git = lambda args: '1360279923'
        self.assertEqual(scm.timestamp_of_native_revision('some-path', '1a1c3b08814bc2a8c15b0f6db63cdce68f2aaa7a'), '2013-02-07T23:32:03Z')

        scm._run_git = lambda args: '1360317321'
        self.assertEqual(scm.timestamp_of_native_revision('some-path', '1a1c3b08814bc2a8c15b0f6db63cdce68f2aaa7a'), '2013-02-08T09:55:21Z')

    def test_svn_data_from_git_svn_id(self):
        scm = self.make_scm()
        scm.find_checkout_root = lambda path: ''
        scm._most_recent_log_matching = lambda grep_str, path: 'git-svn-id: http://svn.webkit.org/repository/webkit/trunk@258024 268f45cc-cd09-0410-ab3c-d52691b4dbfc'
        self.assertEqual(scm.svn_revision(scm.checkout_root), '258024')
        self.assertEqual(scm.svn_branch(scm.checkout_root), 'trunk')

        scm._most_recent_log_matching = lambda grep_str, path: 'git-svn-id: https://svn.webkit.org/repository/webkit/trunk@258024 268f45cc-cd09-0410-ab3c-d52691b4dbfc'
        self.assertEqual(scm.svn_revision(scm.checkout_root), '258024')
        self.assertEqual(scm.svn_branch(scm.checkout_root), 'trunk')

        scm._most_recent_log_matching = lambda grep_str, path: 'git-svn-id: https://svn.webkit.org/repository/webkit/branch/specialSubmission@258000 268f45cc-cd09-0410-ab3c-d52691b4dbfc'
        self.assertEqual(scm.svn_revision(scm.checkout_root), '258000')
        self.assertEqual(scm.svn_branch(scm.checkout_root), 'specialSubmission')
