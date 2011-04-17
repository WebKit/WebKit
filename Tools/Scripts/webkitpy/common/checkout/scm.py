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

import logging
import os
import re
import sys
import shutil

from webkitpy.common.memoized import memoized
from webkitpy.common.system.deprecated_logging import error, log
from webkitpy.common.system.executive import Executive, run_command, ScriptError
from webkitpy.common.system import ospath


def find_checkout_root():
    """Returns the current checkout root (as determined by default_scm().

    Returns the absolute path to the top of the WebKit checkout, or None
    if it cannot be determined.

    """
    scm_system = default_scm()
    if scm_system:
        return scm_system.checkout_root
    return None


def default_scm(patch_directories=None):
    """Return the default SCM object as determined by the CWD and running code.

    Returns the default SCM object for the current working directory; if the
    CWD is not in a checkout, then we attempt to figure out if the SCM module
    itself is part of a checkout, and return that one. If neither is part of
    a checkout, None is returned.

    """
    cwd = os.getcwd()
    scm_system = detect_scm_system(cwd, patch_directories)
    if not scm_system:
        script_directory = os.path.dirname(os.path.abspath(__file__))
        scm_system = detect_scm_system(script_directory, patch_directories)
        if scm_system:
            log("The current directory (%s) is not a WebKit checkout, using %s" % (cwd, scm_system.checkout_root))
        else:
            error("FATAL: Failed to determine the SCM system for either %s or %s" % (cwd, script_directory))
    return scm_system


def detect_scm_system(path, patch_directories=None):
    absolute_path = os.path.abspath(path)

    if patch_directories == []:
        patch_directories = None

    if SVN.in_working_directory(absolute_path):
        return SVN(cwd=absolute_path, patch_directories=patch_directories)
    
    if Git.in_working_directory(absolute_path):
        return Git(cwd=absolute_path)
    
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


class CheckoutNeedsUpdate(ScriptError):
    def __init__(self, script_args, exit_code, output, cwd):
        ScriptError.__init__(self, script_args=script_args, exit_code=exit_code, output=output, cwd=cwd)


def commit_error_handler(error):
    if re.search("resource out of date", error.output):
        raise CheckoutNeedsUpdate(script_args=error.script_args, exit_code=error.exit_code, output=error.output, cwd=error.cwd)
    Executive.default_error_handler(error)


class AuthenticationError(Exception):
    def __init__(self, server_host, prompt_for_password=False):
        self.server_host = server_host
        self.prompt_for_password = prompt_for_password


class AmbiguousCommitError(Exception):
    def __init__(self, num_local_commits, working_directory_is_clean):
        self.num_local_commits = num_local_commits
        self.working_directory_is_clean = working_directory_is_clean


# SCM methods are expected to return paths relative to self.checkout_root.
class SCM:
    def __init__(self, cwd, executive=None):
        self.cwd = cwd
        self.checkout_root = self.find_checkout_root(self.cwd)
        self.dryrun = False
        self._executive = executive or Executive()

    # A wrapper used by subclasses to create processes.
    def run(self, args, cwd=None, input=None, error_handler=None, return_exit_code=False, return_stderr=True, decode_output=True):
        # FIXME: We should set cwd appropriately.
        return self._executive.run_command(args,
                           cwd=cwd,
                           input=input,
                           error_handler=error_handler,
                           return_exit_code=return_exit_code,
                           return_stderr=return_stderr,
                           decode_output=decode_output)

    # SCM always returns repository relative path, but sometimes we need
    # absolute paths to pass to rm, etc.
    def absolute_path(self, repository_relative_path):
        return os.path.join(self.checkout_root, repository_relative_path)

    # FIXME: This belongs in Checkout, not SCM.
    def scripts_directory(self):
        return os.path.join(self.checkout_root, "Tools", "Scripts")

    # FIXME: This belongs in Checkout, not SCM.
    def script_path(self, script_name):
        return os.path.join(self.scripts_directory(), script_name)

    def ensure_clean_working_directory(self, force_clean):
        if self.working_directory_is_clean():
            return
        if not force_clean:
            # FIXME: Shouldn't this use cwd=self.checkout_root?
            print self.run(self.status_command(), error_handler=Executive.ignore_error)
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

    def run_status_and_extract_filenames(self, status_command, status_regexp):
        filenames = []
        # We run with cwd=self.checkout_root so that returned-paths are root-relative.
        for line in self.run(status_command, cwd=self.checkout_root).splitlines():
            match = re.search(status_regexp, line)
            if not match:
                continue
            # status = match.group('status')
            filename = match.group('filename')
            filenames.append(filename)
        return filenames

    def strip_r_from_svn_revision(self, svn_revision):
        match = re.match("^r(?P<svn_revision>\d+)", unicode(svn_revision))
        if (match):
            return match.group('svn_revision')
        return svn_revision

    def svn_revision_from_commit_text(self, commit_text):
        match = re.search(self.commit_success_regexp(), commit_text, re.MULTILINE)
        return match.group('svn_revision')

    @staticmethod
    def _subclass_must_implement():
        raise NotImplementedError("subclasses must implement")

    @staticmethod
    def in_working_directory(path):
        SCM._subclass_must_implement()

    @staticmethod
    def find_checkout_root(path):
        SCM._subclass_must_implement()

    @staticmethod
    def commit_success_regexp():
        SCM._subclass_must_implement()

    def working_directory_is_clean(self):
        self._subclass_must_implement()

    def clean_working_directory(self):
        self._subclass_must_implement()

    def status_command(self):
        self._subclass_must_implement()

    def add(self, path, return_exit_code=False):
        self._subclass_must_implement()

    def delete(self, path):
        self._subclass_must_implement()

    def changed_files(self, git_commit=None):
        self._subclass_must_implement()

    def changed_files_for_revision(self, revision):
        self._subclass_must_implement()

    def revisions_changing_file(self, path, limit=5):
        self._subclass_must_implement()

    def added_files(self):
        self._subclass_must_implement()

    def conflicted_files(self):
        self._subclass_must_implement()

    def display_name(self):
        self._subclass_must_implement()

    def create_patch(self, git_commit=None, changed_files=None):
        self._subclass_must_implement()

    def committer_email_for_revision(self, revision):
        self._subclass_must_implement()

    def contents_at_revision(self, path, revision):
        self._subclass_must_implement()

    def diff_for_revision(self, revision):
        self._subclass_must_implement()

    def diff_for_file(self, path, log=None):
        self._subclass_must_implement()

    def show_head(self, path):
        self._subclass_must_implement()

    def apply_reverse_diff(self, revision):
        self._subclass_must_implement()

    def revert_files(self, file_paths):
        self._subclass_must_implement()

    def commit_with_message(self, message, username=None, password=None, git_commit=None, force_squash=False, changed_files=None):
        self._subclass_must_implement()

    def svn_commit_log(self, svn_revision):
        self._subclass_must_implement()

    def last_svn_commit_log(self):
        self._subclass_must_implement()

    # Subclasses must indicate if they support local commits,
    # but the SCM baseclass will only call local_commits methods when this is true.
    @staticmethod
    def supports_local_commits():
        SCM._subclass_must_implement()

    def remote_merge_base():
        SCM._subclass_must_implement()

    def commit_locally_with_message(self, message):
        error("Your source control manager does not support local commits.")

    def discard_local_commits(self):
        pass

    def local_commits(self):
        return []


# A mixin class that represents common functionality for SVN and Git-SVN.
class SVNRepository:
    def has_authorization_for_realm(self, realm, home_directory=os.getenv("HOME")):
        # Assumes find and grep are installed.
        if not os.path.isdir(os.path.join(home_directory, ".subversion")):
            return False
        find_args = ["find", ".subversion", "-type", "f", "-exec", "grep", "-q", realm, "{}", ";", "-print"]
        find_output = self.run(find_args, cwd=home_directory, error_handler=Executive.ignore_error).rstrip()
        return find_output and os.path.isfile(os.path.join(home_directory, find_output))


class SVN(SCM, SVNRepository):
    # FIXME: These belong in common.config.urls
    svn_server_host = "svn.webkit.org"
    svn_server_realm = "<http://svn.webkit.org:80> Mac OS Forge"

    def __init__(self, cwd, patch_directories, executive=None):
        SCM.__init__(self, cwd, executive)
        self._bogus_dir = None
        if patch_directories == []:
            # FIXME: ScriptError is for Executive, this should probably be a normal Exception.
            raise ScriptError(script_args=svn_info_args, message='Empty list of patch directories passed to SCM.__init__')
        elif patch_directories == None:
            self._patch_directories = [ospath.relpath(cwd, self.checkout_root)]
        else:
            self._patch_directories = patch_directories

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
        info_output = run_command(svn_info_args).rstrip()
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

    @memoized
    def svn_version(self):
        return self.run(['svn', '--version', '--quiet'])

    def working_directory_is_clean(self):
        return self.run(["svn", "diff"], cwd=self.checkout_root, decode_output=False) == ""

    def clean_working_directory(self):
        # Make sure there are no locks lying around from a previously aborted svn invocation.
        # This is slightly dangerous, as it's possible the user is running another svn process
        # on this checkout at the same time.  However, it's much more likely that we're running
        # under windows and svn just sucks (or the user interrupted svn and it failed to clean up).
        self.run(["svn", "cleanup"], cwd=self.checkout_root)

        # svn revert -R is not as awesome as git reset --hard.
        # It will leave added files around, causing later svn update
        # calls to fail on the bots.  We make this mirror git reset --hard
        # by deleting any added files as well.
        added_files = reversed(sorted(self.added_files()))
        # added_files() returns directories for SVN, we walk the files in reverse path
        # length order so that we remove files before we try to remove the directories.
        self.run(["svn", "revert", "-R", "."], cwd=self.checkout_root)
        for path in added_files:
            # This is robust against cwd != self.checkout_root
            absolute_path = self.absolute_path(path)
            # Completely lame that there is no easy way to remove both types with one call.
            if os.path.isdir(path):
                os.rmdir(absolute_path)
            else:
                os.remove(absolute_path)

    def status_command(self):
        return ['svn', 'status']

    def _status_regexp(self, expected_types):
        field_count = 6 if self.svn_version() > "1.6" else 5
        return "^(?P<status>[%s]).{%s} (?P<filename>.+)$" % (expected_types, field_count)

    def _add_parent_directories(self, path):
        """Does 'svn add' to the path and its parents."""
        if self.in_working_directory(path):
            return
        dirname = os.path.dirname(path)
        # We have dirname directry - ensure it added.
        if dirname != path:
            self._add_parent_directories(dirname)
        self.add(path)

    def add(self, path, return_exit_code=False):
        self._add_parent_directories(os.path.dirname(os.path.abspath(path)))
        return self.run(["svn", "add", path], return_exit_code=return_exit_code)

    def delete(self, path):
        parent, base = os.path.split(os.path.abspath(path))
        return self.run(["svn", "delete", "--force", base], cwd=parent)

    def changed_files(self, git_commit=None):
        status_command = ["svn", "status"]
        status_command.extend(self._patch_directories)
        # ACDMR: Addded, Conflicted, Deleted, Modified or Replaced
        return self.run_status_and_extract_filenames(status_command, self._status_regexp("ACDMR"))

    def changed_files_for_revision(self, revision):
        # As far as I can tell svn diff --summarize output looks just like svn status output.
        # No file contents printed, thus utf-8 auto-decoding in self.run is fine.
        status_command = ["svn", "diff", "--summarize", "-c", revision]
        return self.run_status_and_extract_filenames(status_command, self._status_regexp("ACDMR"))

    def revisions_changing_file(self, path, limit=5):
        revisions = []
        # svn log will exit(1) (and thus self.run will raise) if the path does not exist.
        log_command = ['svn', 'log', '--quiet', '--limit=%s' % limit, path]
        for line in self.run(log_command, cwd=self.checkout_root).splitlines():
            match = re.search('^r(?P<revision>\d+) ', line)
            if not match:
                continue
            revisions.append(int(match.group('revision')))
        return revisions

    def conflicted_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("C"))

    def added_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("A"))

    def deleted_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("D"))

    @staticmethod
    def supports_local_commits():
        return False

    def display_name(self):
        return "svn"

    # FIXME: This method should be on Checkout.
    def create_patch(self, git_commit=None, changed_files=None):
        """Returns a byte array (str()) representing the patch file.
        Patch files are effectively binary since they may contain
        files of multiple different encodings."""
        if changed_files == []:
            return ""
        elif changed_files == None:
            changed_files = []
        return self.run([self.script_path("svn-create-patch")] + changed_files,
            cwd=self.checkout_root, return_stderr=False,
            decode_output=False)

    def committer_email_for_revision(self, revision):
        return self.run(["svn", "propget", "svn:author", "--revprop", "-r", revision]).rstrip()

    def contents_at_revision(self, path, revision):
        """Returns a byte array (str()) containing the contents
        of path @ revision in the repository."""
        remote_path = "%s/%s" % (self._repository_url(), path)
        return self.run(["svn", "cat", "-r", revision, remote_path], decode_output=False)

    def diff_for_revision(self, revision):
        # FIXME: This should probably use cwd=self.checkout_root
        return self.run(['svn', 'diff', '-c', revision])

    def _bogus_dir_name(self):
        if sys.platform.startswith("win"):
            parent_dir = tempfile.gettempdir()
        else:
            parent_dir = sys.path[0]  # tempdir is not secure.
        return os.path.join(parent_dir, "temp_svn_config")

    def _setup_bogus_dir(self, log):
        self._bogus_dir = self._bogus_dir_name()
        if not os.path.exists(self._bogus_dir):
            os.mkdir(self._bogus_dir)
            self._delete_bogus_dir = True
        else:
            self._delete_bogus_dir = False
        if log:
            log.debug('  Html: temp config dir: "%s".', self._bogus_dir)

    def _teardown_bogus_dir(self, log):
        if self._delete_bogus_dir:
            shutil.rmtree(self._bogus_dir, True)
            if log:
                log.debug('  Html: removed temp config dir: "%s".', self._bogus_dir)
        self._bogus_dir = None

    def diff_for_file(self, path, log=None):
        self._setup_bogus_dir(log)
        try:
            args = ['svn', 'diff']
            if self._bogus_dir:
                args += ['--config-dir', self._bogus_dir]
            args.append(path)
            return self.run(args)
        finally:
            self._teardown_bogus_dir(log)

    def show_head(self, path):
        return self.run(['svn', 'cat', '-r', 'BASE', path], decode_output=False)

    def _repository_url(self):
        return self.value_from_svn_info(self.checkout_root, 'URL')

    def apply_reverse_diff(self, revision):
        # '-c -revision' applies the inverse diff of 'revision'
        svn_merge_args = ['svn', 'merge', '--non-interactive', '-c', '-%s' % revision, self._repository_url()]
        log("WARNING: svn merge has been known to take more than 10 minutes to complete.  It is recommended you use git for rollouts.")
        log("Running '%s'" % " ".join(svn_merge_args))
        # FIXME: Should this use cwd=self.checkout_root?
        self.run(svn_merge_args)

    def revert_files(self, file_paths):
        # FIXME: This should probably use cwd=self.checkout_root.
        self.run(['svn', 'revert'] + file_paths)

    def commit_with_message(self, message, username=None, password=None, git_commit=None, force_squash=False, changed_files=None):
        # git-commit and force are not used by SVN.
        svn_commit_args = ["svn", "commit"]

        if not username and not self.has_authorization_for_realm(self.svn_server_realm):
            raise AuthenticationError(self.svn_server_host)
        if username:
            svn_commit_args.extend(["--username", username])

        svn_commit_args.extend(["-m", message])

        if changed_files:
            svn_commit_args.extend(changed_files)

        if self.dryrun:
            _log = logging.getLogger("webkitpy.common.system")
            _log.debug('Would run SVN command: "' + " ".join(svn_commit_args) + '"')

            # Return a string which looks like a commit so that things which parse this output will succeed.
            return "Dry run, no commit.\nCommitted revision 0."

        return self.run(svn_commit_args, cwd=self.checkout_root, error_handler=commit_error_handler)

    def svn_commit_log(self, svn_revision):
        svn_revision = self.strip_r_from_svn_revision(svn_revision)
        return self.run(['svn', 'log', '--non-interactive', '--revision', svn_revision])

    def last_svn_commit_log(self):
        # BASE is the checkout revision, HEAD is the remote repository revision
        # http://svnbook.red-bean.com/en/1.0/ch03s03.html
        return self.svn_commit_log('BASE')

    def propset(self, pname, pvalue, path):
        dir, base = os.path.split(path)
        return self.run(['svn', 'pset', pname, pvalue, base], cwd=dir)

    def propget(self, pname, path):
        dir, base = os.path.split(path)
        return self.run(['svn', 'pget', pname, base], cwd=dir).encode('utf-8').rstrip("\n")


# All git-specific logic should go here.
class Git(SCM, SVNRepository):
    def __init__(self, cwd, executive=None):
        SCM.__init__(self, cwd, executive)
        self._check_git_architecture()

    def _machine_is_64bit(self):
        import platform
        # This only is tested on Mac.
        if not platform.mac_ver()[0]:
            return False

        # platform.architecture()[0] can be '64bit' even if the machine is 32bit:
        # http://mail.python.org/pipermail/pythonmac-sig/2009-September/021648.html
        # Use the sysctl command to find out what the processor actually supports.
        return self.run(['sysctl', '-n', 'hw.cpu64bit_capable']).rstrip() == '1'

    def _executable_is_64bit(self, path):
        # Again, platform.architecture() fails us.  On my machine
        # git_bits = platform.architecture(executable=git_path, bits='default')[0]
        # git_bits is just 'default', meaning the call failed.
        file_output = self.run(['file', path])
        return re.search('x86_64', file_output)

    def _check_git_architecture(self):
        if not self._machine_is_64bit():
            return

        # We could path-search entirely in python or with
        # which.py (http://code.google.com/p/which), but this is easier:
        git_path = self.run(['which', 'git']).rstrip()
        if self._executable_is_64bit(git_path):
            return

        webkit_dev_thead_url = "https://lists.webkit.org/pipermail/webkit-dev/2010-December/015249.html"
        log("Warning: This machine is 64-bit, but the git binary (%s) does not support 64-bit.\nInstall a 64-bit git for better performance, see:\n%s\n" % (git_path, webkit_dev_thead_url))

    @classmethod
    def in_working_directory(cls, path):
        return run_command(['git', 'rev-parse', '--is-inside-work-tree'], cwd=path, error_handler=Executive.ignore_error).rstrip() == "true"

    @classmethod
    def find_checkout_root(cls, path):
        # "git rev-parse --show-cdup" would be another way to get to the root
        (checkout_root, dot_git) = os.path.split(run_command(['git', 'rev-parse', '--git-dir'], cwd=(path or "./")))
        # If we were using 2.6 # checkout_root = os.path.relpath(checkout_root, path)
        if not os.path.isabs(checkout_root): # Sometimes git returns relative paths
            checkout_root = os.path.join(path, checkout_root)
        return checkout_root

    @classmethod
    def to_object_name(cls, filepath):
        root_end_with_slash = os.path.join(cls.find_checkout_root(os.path.dirname(filepath)), '')
        return filepath.replace(root_end_with_slash, '')

    @classmethod
    def read_git_config(cls, key):
        # FIXME: This should probably use cwd=self.checkout_root.
        # Pass --get-all for cases where the config has multiple values
        return run_command(["git", "config", "--get-all", key],
            error_handler=Executive.ignore_error).rstrip('\n')

    @staticmethod
    def commit_success_regexp():
        return "^Committed r(?P<svn_revision>\d+)$"

    def discard_local_commits(self):
        # FIXME: This should probably use cwd=self.checkout_root
        self.run(['git', 'reset', '--hard', self.remote_branch_ref()])
    
    def local_commits(self):
        # FIXME: This should probably use cwd=self.checkout_root
        return self.run(['git', 'log', '--pretty=oneline', 'HEAD...' + self.remote_branch_ref()]).splitlines()

    def rebase_in_progress(self):
        return os.path.exists(os.path.join(self.checkout_root, '.git/rebase-apply'))

    def working_directory_is_clean(self):
        # FIXME: This should probably use cwd=self.checkout_root
        return self.run(['git', 'diff', 'HEAD', '--name-only']) == ""

    def clean_working_directory(self):
        # FIXME: These should probably use cwd=self.checkout_root.
        # Could run git clean here too, but that wouldn't match working_directory_is_clean
        self.run(['git', 'reset', '--hard', 'HEAD'])
        # Aborting rebase even though this does not match working_directory_is_clean
        if self.rebase_in_progress():
            self.run(['git', 'rebase', '--abort'])

    def status_command(self):
        # git status returns non-zero when there are changes, so we use git diff name --name-status HEAD instead.
        # No file contents printed, thus utf-8 autodecoding in self.run is fine.
        return ["git", "diff", "--name-status", "HEAD"]

    def _status_regexp(self, expected_types):
        return '^(?P<status>[%s])\t(?P<filename>.+)$' % expected_types

    def add(self, path, return_exit_code=False):
        return self.run(["git", "add", path], return_exit_code=return_exit_code)

    def delete(self, path):
        return self.run(["git", "rm", "-f", path])

    def merge_base(self, git_commit):
        if git_commit:
            # Special-case HEAD.. to mean working-copy changes only.
            if git_commit.upper() == 'HEAD..':
                return 'HEAD'

            if '..' not in git_commit:
                git_commit = git_commit + "^.." + git_commit
            return git_commit

        return self.remote_merge_base()

    def changed_files(self, git_commit=None):
        # FIXME: --diff-filter could be used to avoid the "extract_filenames" step.
        status_command = ['git', 'diff', '-r', '--name-status', '-C', '-M', "--no-ext-diff", "--full-index", self.merge_base(git_commit)]
        # FIXME: I'm not sure we're returning the same set of files that SVN.changed_files is.
        # Added (A), Copied (C), Deleted (D), Modified (M), Renamed (R)
        return self.run_status_and_extract_filenames(status_command, self._status_regexp("ADM"))

    def _changes_files_for_commit(self, git_commit):
        # --pretty="format:" makes git show not print the commit log header,
        changed_files = self.run(["git", "show", "--pretty=format:", "--name-only", git_commit]).splitlines()
        # instead it just prints a blank line at the top, so we skip the blank line:
        return changed_files[1:]

    def changed_files_for_revision(self, revision):
        commit_id = self.git_commit_from_svn_revision(revision)
        return self._changes_files_for_commit(commit_id)

    def revisions_changing_file(self, path, limit=5):
        # git rev-list head --remove-empty --limit=5 -- path would be equivalent.
        commit_ids = self.run(["git", "log", "--remove-empty", "--pretty=format:%H", "-%s" % limit, "--", path]).splitlines()
        return filter(lambda revision: revision, map(self.svn_revision_from_git_commit, commit_ids))

    def conflicted_files(self):
        # We do not need to pass decode_output for this diff command
        # as we're passing --name-status which does not output any data.
        status_command = ['git', 'diff', '--name-status', '-C', '-M', '--diff-filter=U']
        return self.run_status_and_extract_filenames(status_command, self._status_regexp("U"))

    def added_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("A"))

    def deleted_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("D"))

    @staticmethod
    def supports_local_commits():
        return True

    def display_name(self):
        return "git"

    def prepend_svn_revision(self, diff):
        git_log = self.run(['git', 'log', '-25'])
        match = re.search("^\s*git-svn-id:.*@(?P<svn_revision>\d+)\ ", git_log, re.MULTILINE)
        if not match:
            return diff

        return "Subversion Revision: " + str(match.group('svn_revision')) + '\n' + diff

    def create_patch(self, git_commit=None, changed_files=None):
        """Returns a byte array (str()) representing the patch file.
        Patch files are effectively binary since they may contain
        files of multiple different encodings."""
        command = ['git', 'diff', '--binary', "--no-ext-diff", "--full-index", "-M", self.merge_base(git_commit), "--"]
        if changed_files:
            command += changed_files
        return self.prepend_svn_revision(self.run(command, decode_output=False, cwd=self.checkout_root))

    def _run_git_svn_find_rev(self, arg):
        # git svn find-rev always exits 0, even when the revision or commit is not found.
        return self.run(['git', 'svn', 'find-rev', arg], cwd=self.checkout_root).rstrip()

    def _string_to_int_or_none(self, string):
        try:
            return int(string)
        except ValueError, e:
            return None

    @memoized
    def git_commit_from_svn_revision(self, svn_revision):
        git_commit = self._run_git_svn_find_rev('r%s' % svn_revision)
        if not git_commit:
            # FIXME: Alternatively we could offer to update the checkout? Or return None?
            raise ScriptError(message='Failed to find git commit for revision %s, your checkout likely needs an update.' % svn_revision)
        return git_commit

    @memoized
    def svn_revision_from_git_commit(self, git_commit):
        svn_revision = self._run_git_svn_find_rev(git_commit)
        return self._string_to_int_or_none(svn_revision)

    def contents_at_revision(self, path, revision):
        """Returns a byte array (str()) containing the contents
        of path @ revision in the repository."""
        return self.run(["git", "show", "%s:%s" % (self.git_commit_from_svn_revision(revision), path)], decode_output=False)

    def diff_for_revision(self, revision):
        git_commit = self.git_commit_from_svn_revision(revision)
        return self.create_patch(git_commit)

    def diff_for_file(self, path, log=None):
        return self.run(['git', 'diff', 'HEAD', '--', path])

    def show_head(self, path):
        return self.run(['git', 'show', 'HEAD:' + self.to_object_name(path)], decode_output=False)

    def committer_email_for_revision(self, revision):
        git_commit = self.git_commit_from_svn_revision(revision)
        committer_email = self.run(["git", "log", "-1", "--pretty=format:%ce", git_commit])
        # Git adds an extra @repository_hash to the end of every committer email, remove it:
        return committer_email.rsplit("@", 1)[0]

    def apply_reverse_diff(self, revision):
        # Assume the revision is an svn revision.
        git_commit = self.git_commit_from_svn_revision(revision)
        # I think this will always fail due to ChangeLogs.
        self.run(['git', 'revert', '--no-commit', git_commit], error_handler=Executive.ignore_error)

    def revert_files(self, file_paths):
        self.run(['git', 'checkout', 'HEAD'] + file_paths)

    def _assert_can_squash(self, working_directory_is_clean):
        squash = Git.read_git_config('webkit-patch.commit-should-always-squash')
        should_squash = squash and squash.lower() == "true"

        if not should_squash:
            # Only warn if there are actually multiple commits to squash.
            num_local_commits = len(self.local_commits())
            if num_local_commits > 1 or (num_local_commits > 0 and not working_directory_is_clean):
                raise AmbiguousCommitError(num_local_commits, working_directory_is_clean)

    def commit_with_message(self, message, username=None, password=None, git_commit=None, force_squash=False, changed_files=None):
        # Username is ignored during Git commits.
        working_directory_is_clean = self.working_directory_is_clean()

        if git_commit:
            # Special-case HEAD.. to mean working-copy changes only.
            if git_commit.upper() == 'HEAD..':
                if working_directory_is_clean:
                    raise ScriptError(message="The working copy is not modified. --git-commit=HEAD.. only commits working copy changes.")
                self.commit_locally_with_message(message)
                return self._commit_on_branch(message, 'HEAD', username=username, password=password)

            # Need working directory changes to be committed so we can checkout the merge branch.
            if not working_directory_is_clean:
                # FIXME: webkit-patch land will modify the ChangeLogs to correct the reviewer.
                # That will modify the working-copy and cause us to hit this error.
                # The ChangeLog modification could be made to modify the existing local commit.
                raise ScriptError(message="Working copy is modified. Cannot commit individual git_commits.")
            return self._commit_on_branch(message, git_commit, username=username, password=password)

        if not force_squash:
            self._assert_can_squash(working_directory_is_clean)
        self.run(['git', 'reset', '--soft', self.remote_merge_base()])
        self.commit_locally_with_message(message)
        return self.push_local_commits_to_server(username=username, password=password)

    def _commit_on_branch(self, message, git_commit, username=None, password=None):
        branch_ref = self.run(['git', 'symbolic-ref', 'HEAD']).strip()
        branch_name = branch_ref.replace('refs/heads/', '')
        commit_ids = self.commit_ids_from_commitish_arguments([git_commit])

        # We want to squash all this branch's commits into one commit with the proper description.
        # We do this by doing a "merge --squash" into a new commit branch, then dcommitting that.
        MERGE_BRANCH_NAME = 'webkit-patch-land'
        self.delete_branch(MERGE_BRANCH_NAME)

        # We might be in a directory that's present in this branch but not in the
        # trunk.  Move up to the top of the tree so that git commands that expect a
        # valid CWD won't fail after we check out the merge branch.
        os.chdir(self.checkout_root)

        # Stuff our change into the merge branch.
        # We wrap in a try...finally block so if anything goes wrong, we clean up the branches.
        commit_succeeded = True
        try:
            self.run(['git', 'checkout', '-q', '-b', MERGE_BRANCH_NAME, self.remote_branch_ref()])

            for commit in commit_ids:
                # We're on a different branch now, so convert "head" to the branch name.
                commit = re.sub(r'(?i)head', branch_name, commit)
                # FIXME: Once changed_files and create_patch are modified to separately handle each
                # commit in a commit range, commit each cherry pick so they'll get dcommitted separately.
                self.run(['git', 'cherry-pick', '--no-commit', commit])

            self.run(['git', 'commit', '-m', message])
            output = self.push_local_commits_to_server(username=username, password=password)
        except Exception, e:
            log("COMMIT FAILED: " + str(e))
            output = "Commit failed."
            commit_succeeded = False
        finally:
            # And then swap back to the original branch and clean up.
            self.clean_working_directory()
            self.run(['git', 'checkout', '-q', branch_name])
            self.delete_branch(MERGE_BRANCH_NAME)

        return output

    def svn_commit_log(self, svn_revision):
        svn_revision = self.strip_r_from_svn_revision(svn_revision)
        return self.run(['git', 'svn', 'log', '-r', svn_revision])

    def last_svn_commit_log(self):
        return self.run(['git', 'svn', 'log', '--limit=1'])

    # Git-specific methods:
    def _branch_ref_exists(self, branch_ref):
        return self.run(['git', 'show-ref', '--quiet', '--verify', branch_ref], return_exit_code=True) == 0

    def delete_branch(self, branch_name):
        if self._branch_ref_exists('refs/heads/' + branch_name):
            self.run(['git', 'branch', '-D', branch_name])

    def remote_merge_base(self):
        return self.run(['git', 'merge-base', self.remote_branch_ref(), 'HEAD']).strip()

    def remote_branch_ref(self):
        # Use references so that we can avoid collisions, e.g. we don't want to operate on refs/heads/trunk if it exists.
        remote_branch_refs = Git.read_git_config('svn-remote.svn.fetch')
        if not remote_branch_refs:
            remote_master_ref = 'refs/remotes/origin/master'
            if not self._branch_ref_exists(remote_master_ref):
                raise ScriptError(message="Can't find a branch to diff against. svn-remote.svn.fetch is not in the git config and %s does not exist" % remote_master_ref)
            return remote_master_ref

        # FIXME: What's the right behavior when there are multiple svn-remotes listed?
        # For now, just use the first one.
        first_remote_branch_ref = remote_branch_refs.split('\n')[0]
        return first_remote_branch_ref.split(':')[1]

    def commit_locally_with_message(self, message):
        self.run(['git', 'commit', '--all', '-F', '-'], input=message)

    def push_local_commits_to_server(self, username=None, password=None):
        dcommit_command = ['git', 'svn', 'dcommit']
        if self.dryrun:
            dcommit_command.append('--dry-run')
        if not self.has_authorization_for_realm(SVN.svn_server_realm):
            raise AuthenticationError(SVN.svn_server_host, prompt_for_password=True)
        if username:
            dcommit_command.extend(["--username", username])
        output = self.run(dcommit_command, error_handler=commit_error_handler, input=password)
        # Return a string which looks like a commit so that things which parse this output will succeed.
        if self.dryrun:
            output += "\nCommitted r0"
        return output

    # This function supports the following argument formats:
    # no args : rev-list trunk..HEAD
    # A..B    : rev-list A..B
    # A...B   : error!
    # A B     : [A, B]  (different from git diff, which would use "rev-list A..B")
    def commit_ids_from_commitish_arguments(self, args):
        if not len(args):
            args.append('%s..HEAD' % self.remote_branch_ref())

        commit_ids = []
        for commitish in args:
            if '...' in commitish:
                raise ScriptError(message="'...' is not supported (found in '%s'). Did you mean '..'?" % commitish)
            elif '..' in commitish:
                commit_ids += reversed(self.run(['git', 'rev-list', commitish]).splitlines())
            else:
                # Turn single commits or branch or tag names into commit ids.
                commit_ids += self.run(['git', 'rev-parse', '--revs-only', commitish]).splitlines()
        return commit_ids

    def commit_message_for_local_commit(self, commit_id):
        commit_lines = self.run(['git', 'cat-file', 'commit', commit_id]).splitlines()

        # Skip the git headers.
        first_line_after_headers = 0
        for line in commit_lines:
            first_line_after_headers += 1
            if line == "":
                break
        return CommitMessage(commit_lines[first_line_after_headers:])

    def files_changed_summary_for_commit(self, commit_id):
        return self.run(['git', 'diff-tree', '--shortstat', '--no-commit-id', commit_id])
