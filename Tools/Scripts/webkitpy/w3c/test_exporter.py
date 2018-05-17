# Copyright (c) 2017, Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
 This script uploads changes made to W3C web-platform-tests tests.
"""

import argparse
import logging
import os
import time

from webkitpy.common.checkout.scm.git import Git
from webkitpy.common.host import Host
from webkitpy.common.net.bugzilla import Bugzilla
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.w3c.wpt_github import WPTGitHub
from webkitpy.w3c.wpt_linter import WPTLinter

_log = logging.getLogger(__name__)

WEBKIT_WPT_DIR = 'LayoutTests/imported/w3c/web-platform-tests'
WPT_PR_URL = "https://github.com/w3c/web-platform-tests/pull/"
WEBKIT_EXPORT_PR_LABEL = 'webkit-export'


class TestExporter(object):

    def __init__(self, host, options, gitClass=Git, bugzillaClass=Bugzilla, WPTGitHubClass=WPTGitHub, WPTLinterClass=WPTLinter):
        self._host = host
        self._filesystem = host.filesystem
        self._options = options

        self._host.initialize_scm()

        self._bugzilla = bugzillaClass()
        self._bug_id = options.bug_id
        if not self._bug_id:
            if options.attachment_id:
                self._bug_id = self._bugzilla.bug_id_for_attachment_id(options.attachment_id)
            elif options.git_commit:
                self._bug_id = self._host.checkout().bug_id_for_this_commit(options.git_commit)

        if not self._options.repository_directory:
            webkit_finder = WebKitFinder(self._filesystem)
            self._options.repository_directory = webkit_finder.path_from_webkit_base('WebKitBuild', 'w3c-tests', 'web-platform-tests')

        self._git = self._ensure_wpt_repository("https://github.com/w3c/web-platform-tests.git", self._options.repository_directory, gitClass)
        self._linter = WPTLinterClass(self._options.repository_directory, host.filesystem)

        self._username = options.username
        if not self._username:
            self._username = self._git.local_config('github.username').rstrip()
            if not self._username:
                self._username = os.environ.get('GITHUB_USERNAME')
            if not self._username:
                raise ValueError("Missing GitHub username, please provide it as a command argument (see help for the command).")
        elif not self._git.local_config('github.username'):
            self._git.set_local_config('github.username', self._username)

        self._token = options.token
        if not self._token:
            self._token = self._git.local_config('github.token').rstrip()
            if not self._token:
                self._token = os.environ.get('GITHUB_TOKEN')
            if not self._token:
                _log.info("Missing GitHub token, the script will not be able to create a pull request to W3C web-platform-tests repository.")
        elif not self._git.local_config('github.token'):
            self._git.set_local_config('github.token', self._token)

        self._github = WPTGitHubClass(self._host, self._username, self._token) if self._username and self._token else None

        self._branch_name = self._ensure_new_branch_name()
        self._public_branch_name = options.public_branch_name if options.public_branch_name else self._branch_name
        self._bugzilla_url = "https://bugs.webkit.org/show_bug.cgi?id=" + str(self._bug_id)
        self._commit_message = options.message
        if not self._commit_message:
            self._commit_message = 'WebKit export of ' + self._bugzilla_url if self._bug_id else 'Export made from a WebKit repository'

        self._wpt_fork_remote = options.repository_remote
        if not self._wpt_fork_remote:
            self._wpt_fork_remote = self._username

        self._wpt_fork_push_url = options.repository_remote_url
        if not self._wpt_fork_push_url:
            self._wpt_fork_push_url = "https://" + self._username + "@github.com/" + self._username + "/web-platform-tests.git"

    def _ensure_wpt_repository(self, url, wpt_repository_directory, gitClass):
        git = None
        if not self._filesystem.exists(wpt_repository_directory):
            _log.info('Cloning %s into %s...' % (url, wpt_repository_directory))
            gitClass.clone(url, wpt_repository_directory, self._host.executive)
        git = gitClass(wpt_repository_directory, None, executive=self._host.executive, filesystem=self._filesystem)
        return git

    def _fetch_wpt_repository(self):
        _log.info('Fetching web-platform-tests repository')
        self._git.fetch()

    def _ensure_new_branch_name(self):
        branch_name_prefix = "wpt-export-for-webkit-" + (str(self._bug_id) if self._bug_id else "0")
        branch_name = branch_name_prefix
        counter = 0
        while self._git.branch_ref_exists(branch_name):
            branch_name = ("%s-%s") % (branch_name_prefix, str(counter))
            counter = counter + 1
        return branch_name

    def download_and_commit_patch(self):
        if self._options.git_commit:
            return True

        patch_options = ["--no-update", "--no-clean", "--local-commit"]
        if self._options.attachment_id:
            patch_options.insert("apply-attachment")
            patch_options.append(self._options.attachment_id)
        elif self._options.bug_id:
            patch_options.insert("apply-from-bug")
            patch_options.append(self._options.bug_id)
        else:
            _log.info("Exporting local changes")
            return
        raise TypeError("Retrieval of patch from bugzilla is not yet implemented")

    def clean(self):
        _log.info('Cleaning web-platform-tests master branch')
        self._git.checkout('master')
        self._git.reset_hard('origin/master')

    def create_branch_with_patch(self, patch):
        _log.info('Applying patch to web-platform-tests branch ' + self._branch_name)
        try:
            self._git.checkout_new_branch(self._branch_name)
        except Exception as e:
            _log.warning(e)
            _log.info("Retrying to create the branch")
            self._git.delete_branch(self._branch_name)
            self._git.checkout_new_branch(self._branch_name)
        try:
            self._git.apply_mail_patch([patch, '--exclude', '*-expected.txt', '--exclude', '*.worker.html', '--exclude', '*.any.html', '--exclude', '*.any.worker.html'])
        except Exception as e:
            _log.warning(e)
            self._git.apply_mail_patch(['--abort'])
            return False
        self._git.commit(['-a', '-m', self._commit_message])
        return True

    def push_to_wpt_fork(self):
        self.create_upload_remote_if_needed()
        wpt_fork_branch_github_url = "https://github.com/" + self._username + "/web-platform-tests/tree/" + self._public_branch_name
        _log.info('Pushing branch ' + self._branch_name + " to " + self._git.remote(["get-url", self._wpt_fork_remote]).rstrip())
        _log.info('This may take some time')
        self._git.push([self._wpt_fork_remote, self._branch_name + ":" + self._public_branch_name, '-f'])
        _log.info('Branch available at ' + wpt_fork_branch_github_url)
        return True

    def make_pull_request(self):
        if not self._github:
            _log.info('Missing information to create a pull request')
            return

        _log.info('Making pull request')
        description = self._bugzilla.fetch_bug_dictionary(self._bug_id)["title"]
        pr_number = self.create_wpt_pull_request(self._wpt_fork_remote + ':' + self._public_branch_name, self._commit_message, self._commit_message + "\n" + description)
        if pr_number:
            try:
                self._github.add_label(pr_number, WEBKIT_EXPORT_PR_LABEL)
            except Exception as e:
                _log.warning(e)
                _log.info('Could not add label "%s" to pr #%s. User "%s" may not have permission to update labels in the w3c/web-platform-test repo.' % (WEBKIT_EXPORT_PR_LABEL, pr_number, self._username))
        if self._bug_id and pr_number:
            self._bugzilla.post_comment_to_bug(self._bug_id, "Submitted web-platform-tests pull request: " + WPT_PR_URL + str(pr_number))

    def create_wpt_pull_request(self, remote_branch_name, title, body):
        pr_number = None
        try:
            pr_number = self._github.create_pr(remote_branch_name, title, body)
        except Exception as e:
            if e.code == 422:
                _log.info('Unable to create a new pull request for branch "%s" because a pull request already exists. The branch has been updated and there is no further action needed.' % (remote_branch_name))
            else:
                _log.warning(e)
                _log.info('Error creating a pull request on github. Please ensure that the provided github token has the "public_repo" scope.')
        return pr_number

    def delete_local_branch(self):
        _log.info('Removing branch ' + self._branch_name)
        self._git.checkout('master')
        self._git.delete_branch(self._branch_name)

    def create_git_patch(self):
        patch_file = './patch.temp.' + str(time.clock())
        git_commit = "HEAD...." if not self._options.git_commit else self._options.git_commit + "~1.." + self._options.git_commit
        patch_data = self._host.scm().create_patch(git_commit, [WEBKIT_WPT_DIR])
        if not patch_data or not 'diff' in patch_data:
            _log.info('No changes to upstream, patch data is: "%s"' % (patch_data))
            return ''
        # FIXME: We can probably try to use --relative git parameter to not do that replacement.
        patch_data = patch_data.replace(WEBKIT_WPT_DIR + '/', '')
        patch_file = self._filesystem.abspath(patch_file)
        self._filesystem.write_text_file(patch_file, patch_data)
        return patch_file

    def create_upload_remote_if_needed(self):
        if not self._wpt_fork_remote in self._git.remote([]):
            self._git.remote(["add", self._wpt_fork_remote, self._wpt_fork_push_url])

    def do_export(self):
        git_patch_file = self.create_git_patch()

        if not git_patch_file:
            _log.error("Unable to create a patch to apply to web-platform-tests repository")
            return

        self._fetch_wpt_repository()
        self.clean()

        if not self.create_branch_with_patch(git_patch_file):
            _log.error("Cannot create web-platform-tests local branch from the patch")
            self.delete_local_branch()
            return

        if git_patch_file:
            self._filesystem.remove(git_patch_file)

        lint_errors = self._linter.lint()
        if lint_errors:
            _log.error("The wpt linter detected %s linting error(s). Please address the above errors before attempting to export changes to the web-platform-test repository." % (lint_errors,))
            self.delete_local_branch()
            self.clean()
            return

        try:
            if self.push_to_wpt_fork():
                if self._options.create_pull_request:
                    self.make_pull_request()
        finally:
            self.delete_local_branch()
            _log.info("Finished")
            self.clean()


def parse_args(args):
    description = """Script to generate a pull request to W3C web-platform-tests repository
    'Tools/Scripts/export-w3c-test-changes -c -g HEAD -b XYZ' will do the following:
    - Clone web-platform-tests repository if not done already and set it up for pushing branches.
    - Gather WebKit bug id XYZ bug and changes to apply to web-platform-tests repository based on the HEAD commit
    - Create a remote branch named webkit-XYZ on https://github.com/USERNAME/web-platform-tests.git repository based on the locally applied patch.
    -    USERNAME may be set using the environment variable GITHUB_USERNAME or as a command line option. It is then stored in git config as github.username.
    -    Github credential may be set using the environment variable GITHUB_TOKEN or as a command line option. (Please provide a valid GitHub 'Personal access token' with 'repo' as scope). It is then stored in git config as github.token.
    - Make the related pull request on https://github.com/w3c/web-platform-tests.git repository.
    - Clean the local Git repository
    Notes:
    - It is safer to provide a bug id using -b option (bug id from a git commit is not always working).
    - As a dry run, one can start by running the script without -c. This will only create the branch on the user public GitHub repository.
    - By default, the script will create an https remote URL that will require a password-based authentication to GitHub. If you are using an SSH key, please use the --remote-url option.
    FIXME:
    - The script is not yet able to update an existing pull request
    - Need a way to monitor the progress of the pul request so that status of all pending pull requests can be done at import time.
    """
    parser = argparse.ArgumentParser(prog='export-w3c-test-changes ...', description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('-g', '--git-commit', dest='git_commit', default=None, help='Git commit to apply')
    parser.add_argument('-b', '--bug', dest='bug_id', default=None, help='Bug ID to search for patch')
    parser.add_argument('-a', '--attachment', dest='attachment_id', default=None, help='Attachment ID to search for patch')
    parser.add_argument('-n', '--name', dest='username', default=None, help='github user name if GITHUB_USERNAME is not defined or github.username in the WPT repo config is not defined')
    parser.add_argument('-t', '--token', dest='token', default=None, help='github token, needed for creating pull requests only if GITHUB_TOKEN env variable is not defined or github.token in the WPT repo config is not defined')
    parser.add_argument('-bn', '--branch-name', dest='public_branch_name', default=None, help='Branch name to push to')
    parser.add_argument('-m', '--message', dest='message', default=None, help='Commit message')
    parser.add_argument('-r', '--remote', dest='repository_remote', default=None, help='repository origin to use to push')
    parser.add_argument('-u', '--remote-url', dest='repository_remote_url', default=None, help='repository url to use to push')
    parser.add_argument('-d', '--repository', dest='repository_directory', default=None, help='repository directory')
    parser.add_argument('-c', '--create-pr', dest='create_pull_request', action='store_true', default=False, help='create pull request to w3c web-platform-tests')

    options, args = parser.parse_known_args(args)

    return options


def configure_logging():
    class LogHandler(logging.StreamHandler):

        def format(self, record):
            if record.levelno > logging.INFO:
                return "%s: %s" % (record.levelname, record.getMessage())
            return record.getMessage()

    logger = logging.getLogger('webkitpy.w3c.test_exporter')
    logger.setLevel(logging.INFO)
    handler = LogHandler()
    handler.setLevel(logging.INFO)
    logger.addHandler(handler)
    return handler


def main(_argv, _stdout, _stderr):
    options = parse_args(_argv)

    configure_logging()

    test_exporter = TestExporter(Host(), options)

    test_exporter.do_export()
