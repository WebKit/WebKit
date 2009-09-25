# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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
#
# Python module for interacting with an SCM system (like SVN or Git)

import os
import re
import subprocess

# Import WebKit-specific modules.
from modules.logging import error, log

def detect_scm_system(path):
    if SVN.in_working_directory(path):
        return SVN(cwd=path)
    
    if Git.in_working_directory(path):
        return Git(cwd=path)
    
    return None

def first_non_empty_line_after_index(lines, index=0):
    first_non_empty_line = index
    for line in lines[index:]:
        if re.match("^\s*$", line):
            first_non_empty_line += 1
        else:
            break
    return first_non_empty_line


class CommitMessage:
    def __init__(self, message):
        self.message_lines = message[first_non_empty_line_after_index(message, 0):]

    def body(self, lstrip=False):
        lines = self.message_lines[first_non_empty_line_after_index(self.message_lines, 1):]
        if lstrip:
            lines = [line.lstrip() for line in lines]
        return "\n".join(lines) + "\n"

    def description(self, lstrip=False, strip_url=False):
        line = self.message_lines[0]
        if lstrip:
            line = line.lstrip()
        if strip_url:
            line = re.sub("^(\s*)<.+> ", "\1", line)
        return line

    def message(self):
        return "\n".join(self.message_lines) + "\n"


class ScriptError(Exception):
    def __init__(self, script_args=None, exit_code=None, message=None, output=None, cwd=None):
        if not message:
            message = 'Failed to run "%s"' % script_args
            if exit_code:
                message += " exit_code: %d" % exit_code
            if cwd:
                message += " cwd: %s" % cwd

        Exception.__init__(self, message)
        self.script_args = script_args # 'args' is already used by Exception
        self.exit_code = exit_code
        self.output = output
        self.cwd = cwd

    def message_with_output(self, output_limit=500):
        if self.output:
            if len(self.output) > output_limit:
                 return "%s\nLast %s characters of output:\n%s" % (self, output_limit, self.output[-output_limit:])
            return "%s\n%s" % (self, self.output)
        return str(self)

class SCM:
    def __init__(self, cwd, dryrun=False):
        self.cwd = cwd
        self.checkout_root = self.find_checkout_root(self.cwd)
        self.dryrun = dryrun

    @staticmethod
    def run_command(args, cwd=None, input=None, raise_on_failure=True, return_exit_code=False):
        stdin = subprocess.PIPE if input else None
        process = subprocess.Popen(args, stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=cwd)
        output = process.communicate(input)[0].rstrip()
        exit_code = process.wait()
        if raise_on_failure and exit_code:
            raise ScriptError(script_args=args, exit_code=exit_code, output=output, cwd=cwd)
        if return_exit_code:
            return exit_code
        return output

    def scripts_directory(self):
        return os.path.join(self.checkout_root, "WebKitTools", "Scripts")

    def script_path(self, script_name):
        return os.path.join(self.scripts_directory(), script_name)

    def ensure_clean_working_directory(self, force):
        if not force and not self.working_directory_is_clean():
            print self.run_command(self.status_command(), raise_on_failure=False)
            raise ScriptError(message="Working directory has modifications, pass --force-clean or --no-clean to continue.")
        
        log("Cleaning working directory")
        self.clean_working_directory()
    
    def ensure_no_local_commits(self, force):
        if not self.supports_local_commits():
            return
        commits = self.local_commits()
        if not len(commits):
            return
        if not force:
            error("Working directory has local commits, pass --force-clean to continue.")
        self.discard_local_commits()

    def apply_patch(self, patch, force=False):
        # It's possible that the patch was not made from the root directory.
        # We should detect and handle that case.
        curl_process = subprocess.Popen(['curl', '--location', '--silent', '--show-error', patch['url']], stdout=subprocess.PIPE)
        args = [self.script_path('svn-apply'), '--reviewer', patch['reviewer']]
        if force:
            args.append('--force')
        patch_apply_process = subprocess.Popen(args, stdin=curl_process.stdout)

        return_code = patch_apply_process.wait()
        if return_code:
            raise ScriptError(message="Patch %s from bug %s failed to download and apply." % (patch['url'], patch['bug_id']))

    def run_status_and_extract_filenames(self, status_command, status_regexp):
        filenames = []
        for line in self.run_command(status_command).splitlines():
            match = re.search(status_regexp, line)
            if not match:
                continue
            # status = match.group('status')
            filename = match.group('filename')
            filenames.append(filename)
        return filenames

    def strip_r_from_svn_revision(self, svn_revision):
        match = re.match("^r(?P<svn_revision>\d+)", svn_revision)
        if (match):
            return match.group('svn_revision')
        return svn_revision

    def svn_revision_from_commit_text(self, commit_text):
        match = re.search(self.commit_success_regexp(), commit_text, re.MULTILINE)
        return match.group('svn_revision')

    # ChangeLog-specific code doesn't really belong in scm.py, but this function is very useful.
    def modified_changelogs(self):
        changelog_paths = []
        paths = self.changed_files()
        for path in paths:
            if os.path.basename(path) == "ChangeLog":
                changelog_paths.append(path)
        return changelog_paths

    @staticmethod
    def in_working_directory(path):
        raise NotImplementedError, "subclasses must implement"

    @staticmethod
    def find_checkout_root(path):
        raise NotImplementedError, "subclasses must implement"

    @staticmethod
    def commit_success_regexp():
        raise NotImplementedError, "subclasses must implement"

    def working_directory_is_clean(self):
        raise NotImplementedError, "subclasses must implement"

    def clean_working_directory(self):
        raise NotImplementedError, "subclasses must implement"

    def update_webkit(self):
        raise NotImplementedError, "subclasses must implement"

    def status_command(self):
        raise NotImplementedError, "subclasses must implement"

    def changed_files(self):
        raise NotImplementedError, "subclasses must implement"

    def display_name(self):
        raise NotImplementedError, "subclasses must implement"

    def create_patch(self):
        raise NotImplementedError, "subclasses must implement"

    def diff_for_revision(self, revision):
        raise NotImplementedError, "subclasses must implement"

    def apply_reverse_diff(self, revision):
        raise NotImplementedError, "subclasses must implement"

    def revert_files(self, file_paths):
        raise NotImplementedError, "subclasses must implement"

    def commit_with_message(self, message):
        raise NotImplementedError, "subclasses must implement"

    def svn_commit_log(self, svn_revision):
        raise NotImplementedError, "subclasses must implement"

    def last_svn_commit_log(self):
        raise NotImplementedError, "subclasses must implement"

    # Subclasses must indicate if they support local commits,
    # but the SCM baseclass will only call local_commits methods when this is true.
    @staticmethod
    def supports_local_commits():
        raise NotImplementedError, "subclasses must implement"

    def create_patch_from_local_commit(self, commit_id):
        error("Your source control manager does not support creating a patch from a local commit.")

    def create_patch_since_local_commit(self, commit_id):
        error("Your source control manager does not support creating a patch from a local commit.")

    def commit_locally_with_message(self, message):
        error("Your source control manager does not support local commits.")

    def discard_local_commits(self):
        pass

    def local_commits(self):
        return []


class SVN(SCM):
    def __init__(self, cwd, dryrun=False):
        SCM.__init__(self, cwd, dryrun)
        self.cached_version = None
    
    @staticmethod
    def in_working_directory(path):
        return os.path.isdir(os.path.join(path, '.svn'))
    
    @classmethod
    def find_uuid(cls, path):
        if not cls.in_working_directory(path):
            return None
        return cls.value_from_svn_info(path, 'Repository UUID')

    @classmethod
    def value_from_svn_info(cls, path, field_name):
        svn_info_args = ['svn', 'info', path]
        info_output = cls.run_command(svn_info_args)
        match = re.search("^%s: (?P<value>.+)$" % field_name, info_output, re.MULTILINE)
        if not match:
            raise ScriptError(script_args=svn_info_args, message='svn info did not contain a %s.' % field_name)
        return match.group('value')

    @staticmethod
    def find_checkout_root(path):
        uuid = SVN.find_uuid(path)
        # If |path| is not in a working directory, we're supposed to return |path|.
        if not uuid:
            return path
        # Search up the directory hierarchy until we find a different UUID.
        last_path = None
        while True:
            if uuid != SVN.find_uuid(path):
                return last_path
            last_path = path
            (path, last_component) = os.path.split(path)
            if last_path == path:
                return None

    @staticmethod
    def commit_success_regexp():
        return "^Committed revision (?P<svn_revision>\d+)\.$"
    
    def svn_version(self):
        if not self.cached_version:
            self.cached_version = self.run_command(['svn', '--version', '--quiet'])
        
        return self.cached_version

    def working_directory_is_clean(self):
        return self.run_command(['svn', 'diff']) == ""

    def clean_working_directory(self):
        self.run_command(['svn', 'revert', '-R', '.'])

    def update_webkit(self):
        self.run_command(self.script_path("update-webkit"))

    def status_command(self):
        return ['svn', 'status']

    def changed_files(self):
        if self.svn_version() > "1.6":
            status_regexp = "^(?P<status>[ACDMR]).{6} (?P<filename>.+)$"
        else:
            status_regexp = "^(?P<status>[ACDMR]).{5} (?P<filename>.+)$"
        return self.run_status_and_extract_filenames(self.status_command(), status_regexp)

    @staticmethod
    def supports_local_commits():
        return False

    def display_name(self):
        return "svn"

    def create_patch(self):
        return self.run_command(self.script_path("svn-create-patch"), cwd=self.checkout_root)

    def diff_for_revision(self, revision):
        return self.run_command(['svn', 'diff', '-c', str(revision)])

    def _repository_url(self):
        return self.value_from_svn_info(self.checkout_root, 'URL')

    def apply_reverse_diff(self, revision):
        # '-c -revision' applies the inverse diff of 'revision'
        svn_merge_args = ['svn', 'merge', '--non-interactive', '-c', '-%s' % revision, self._repository_url()]
        log("WARNING: svn merge has been known to take more than 10 minutes to complete.  It is recommended you use git for rollouts.")
        log("Running '%s'" % " ".join(svn_merge_args))
        self.run_command(svn_merge_args)

    def revert_files(self, file_paths):
        self.run_command(['svn', 'revert'] + file_paths)

    def commit_with_message(self, message):
        if self.dryrun:
            # Return a string which looks like a commit so that things which parse this output will succeed.
            return "Dry run, no commit.\nCommitted revision 0."
        return self.run_command(['svn', 'commit', '-m', message])

    def svn_commit_log(self, svn_revision):
        svn_revision = self.strip_r_from_svn_revision(str(svn_revision))
        return self.run_command(['svn', 'log', '--non-interactive', '--revision', svn_revision]);

    def last_svn_commit_log(self):
        # BASE is the checkout revision, HEAD is the remote repository revision
        # http://svnbook.red-bean.com/en/1.0/ch03s03.html
        return self.svn_commit_log('BASE')

# All git-specific logic should go here.
class Git(SCM):
    def __init__(self, cwd, dryrun=False):
        SCM.__init__(self, cwd, dryrun)

    @classmethod
    def in_working_directory(cls, path):
        return cls.run_command(['git', 'rev-parse', '--is-inside-work-tree'], cwd=path) == "true"

    @classmethod
    def find_checkout_root(cls, path):
        # "git rev-parse --show-cdup" would be another way to get to the root
        (checkout_root, dot_git) = os.path.split(cls.run_command(['git', 'rev-parse', '--git-dir'], cwd=path))
        # If we were using 2.6 # checkout_root = os.path.relpath(checkout_root, path)
        if not os.path.isabs(checkout_root): # Sometimes git returns relative paths
            checkout_root = os.path.join(path, checkout_root)
        return checkout_root
    
    @staticmethod
    def commit_success_regexp():
        return "^Committed r(?P<svn_revision>\d+)$"
    
    def discard_local_commits(self):
        self.run_command(['git', 'reset', '--hard', 'trunk'])
    
    def local_commits(self):
        return self.run_command(['git', 'log', '--pretty=oneline', 'HEAD...trunk']).splitlines()

    def rebase_in_progress(self):
        return os.path.exists(os.path.join(self.checkout_root, '.git/rebase-apply'))

    def working_directory_is_clean(self):
        return self.run_command(['git', 'diff-index', 'HEAD']) == ""

    def clean_working_directory(self):
        # Could run git clean here too, but that wouldn't match working_directory_is_clean
        self.run_command(['git', 'reset', '--hard', 'HEAD'])
        # Aborting rebase even though this does not match working_directory_is_clean
        if self.rebase_in_progress():
            self.run_command(['git', 'rebase', '--abort'])

    def update_webkit(self):
        # FIXME: Call update-webkit once https://bugs.webkit.org/show_bug.cgi?id=27162 is fixed.
        log("Updating working directory")
        self.run_command(['git', 'svn', 'rebase'])

    def status_command(self):
        return ['git', 'status']

    def changed_files(self):
        status_command = ['git', 'diff', '-r', '--name-status', '-C', '-M', 'HEAD']
        status_regexp = '^(?P<status>[ADM])\t(?P<filename>.+)$'
        return self.run_status_and_extract_filenames(status_command, status_regexp)
    
    @staticmethod
    def supports_local_commits():
        return True

    def display_name(self):
        return "git"

    def create_patch(self):
        return self.run_command(['git', 'diff', 'HEAD'])

    @classmethod
    def git_commit_from_svn_revision(cls, revision):
        # git svn find-rev always exits 0, even when the revision is not found.
        return cls.run_command(['git', 'svn', 'find-rev', 'r%s' % revision])

    def diff_for_revision(self, revision):
        git_commit = self.git_commit_from_svn_revision(revision)
        return self.create_patch_from_local_commit(git_commit)

    def apply_reverse_diff(self, revision):
        # Assume the revision is an svn revision.
        git_commit = self.git_commit_from_svn_revision(revision)
        if not git_commit:
            raise ScriptError(message='Failed to find git commit for revision %s, git svn log output: "%s"' % (revision, git_commit))

        # I think this will always fail due to ChangeLogs.
        # FIXME: We need to detec specific failure conditions and handle them.
        self.run_command(['git', 'revert', '--no-commit', git_commit], raise_on_failure=False)

        # Fix any ChangeLogs if necessary.
        changelog_paths = self.modified_changelogs()
        if len(changelog_paths):
            self.run_command([self.script_path('resolve-ChangeLogs')] + changelog_paths)

    def revert_files(self, file_paths):
        self.run_command(['git', 'checkout', 'HEAD'] + file_paths)

    def commit_with_message(self, message):
        self.commit_locally_with_message(message)
        return self.push_local_commits_to_server()

    def svn_commit_log(self, svn_revision):
        svn_revision = self.strip_r_from_svn_revision(svn_revision)
        return self.run_command(['git', 'svn', 'log', '-r', svn_revision])

    def last_svn_commit_log(self):
        return self.run_command(['git', 'svn', 'log', '--limit=1'])

    # Git-specific methods:

    def create_patch_from_local_commit(self, commit_id):
        return self.run_command(['git', 'diff', commit_id + "^.." + commit_id])

    def create_patch_since_local_commit(self, commit_id):
        return self.run_command(['git', 'diff', commit_id])

    def commit_locally_with_message(self, message):
        self.run_command(['git', 'commit', '--all', '-F', '-'], input=message)
        
    def push_local_commits_to_server(self):
        if self.dryrun:
            # Return a string which looks like a commit so that things which parse this output will succeed.
            return "Dry run, no remote commit.\nCommitted r0"
        return self.run_command(['git', 'svn', 'dcommit'])

    # This function supports the following argument formats:
    # no args : rev-list trunk..HEAD
    # A..B    : rev-list A..B
    # A...B   : error!
    # A B     : [A, B]  (different from git diff, which would use "rev-list A..B")
    def commit_ids_from_commitish_arguments(self, args):
        if not len(args):
            # FIXME: trunk is not always the remote branch name, need a way to detect the name.
            args.append('trunk..HEAD')

        commit_ids = []
        for commitish in args:
            if '...' in commitish:
                raise ScriptError(message="'...' is not supported (found in '%s'). Did you mean '..'?" % commitish)
            elif '..' in commitish:
                commit_ids += reversed(self.run_command(['git', 'rev-list', commitish]).splitlines())
            else:
                # Turn single commits or branch or tag names into commit ids.
                commit_ids += self.run_command(['git', 'rev-parse', '--revs-only', commitish]).splitlines()
        return commit_ids

    def commit_message_for_local_commit(self, commit_id):
        commit_lines = self.run_command(['git', 'cat-file', 'commit', commit_id]).splitlines()

        # Skip the git headers.
        first_line_after_headers = 0
        for line in commit_lines:
            first_line_after_headers += 1
            if line == "":
                break
        return CommitMessage(commit_lines[first_line_after_headers:])

    def files_changed_summary_for_commit(self, commit_id):
        return self.run_command(['git', 'diff-tree', '--shortstat', '--no-commit-id', commit_id])
