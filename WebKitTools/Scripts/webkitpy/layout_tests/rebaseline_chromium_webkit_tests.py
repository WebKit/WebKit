#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Rebaselining tool that automatically produces baselines for all platforms.

The script does the following for each platform specified:
  1. Compile a list of tests that need rebaselining.
  2. Download test result archive from buildbot for the platform.
  3. Extract baselines from the archive file for all identified files.
  4. Add new baselines to SVN repository.
  5. For each test that has been rebaselined, remove this platform option from
     the test in test_expectation.txt. If no other platforms remain after
     removal, delete the rebaselined test from the file.

At the end, the script generates a html that compares old and new baselines.
"""

import logging
import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib
import webbrowser
import zipfile

from layout_package import path_utils
from layout_package import test_expectations
from test_types import image_diff
from test_types import text_diff

# Repository type constants.
REPO_SVN, REPO_UNKNOWN = range(2)

BASELINE_SUFFIXES = ['.txt', '.png', '.checksum']
REBASELINE_PLATFORM_ORDER = ['mac', 'win', 'win-xp', 'win-vista', 'linux']
ARCHIVE_DIR_NAME_DICT = {'win': 'webkit-rel',
                         'win-vista': 'webkit-dbg-vista',
                         'win-xp': 'webkit-rel',
                         'mac': 'webkit-rel-mac5',
                         'linux': 'webkit-rel-linux',
                         'win-canary': 'webkit-rel-webkit-org',
                         'win-vista-canary': 'webkit-dbg-vista',
                         'win-xp-canary': 'webkit-rel-webkit-org',
                         'mac-canary': 'webkit-rel-mac-webkit-org',
                         'linux-canary': 'webkit-rel-linux-webkit-org'}


# FIXME: Should be rolled into webkitpy.Executive
def run_shell_with_return_code(command, print_output=False):
    """Executes a command and returns the output and process return code.

    Args:
      command: program and arguments.
      print_output: if true, print the command results to standard output.

    Returns:
      command output, return code
    """

    # Use a shell for subcommands on Windows to get a PATH search.
    use_shell = sys.platform.startswith('win')
    p = subprocess.Popen(command, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, shell=use_shell)
    if print_output:
        output_array = []
        while True:
            line = p.stdout.readline()
            if not line:
                break
            if print_output:
                print line.strip('\n')
            output_array.append(line)
        output = ''.join(output_array)
    else:
        output = p.stdout.read()
    p.wait()
    p.stdout.close()

    return output, p.returncode


# FIXME: Should be rolled into webkitpy.Executive
def run_shell(command, print_output=False):
    """Executes a command and returns the output.

    Args:
      command: program and arguments.
      print_output: if true, print the command results to standard output.

    Returns:
      command output
    """

    output, return_code = run_shell_with_return_code(command, print_output)
    return output


def log_dashed_string(text, platform, logging_level=logging.INFO):
    """Log text message with dashes on both sides."""

    msg = text
    if platform:
        msg += ': ' + platform
    if len(msg) < 78:
        dashes = '-' * ((78 - len(msg)) / 2)
        msg = '%s %s %s' % (dashes, msg, dashes)

    if logging_level == logging.ERROR:
        logging.error(msg)
    elif logging_level == logging.WARNING:
        logging.warn(msg)
    else:
        logging.info(msg)


def setup_html_directory(html_directory):
    """Setup the directory to store html results.

       All html related files are stored in the "rebaseline_html" subdirectory.

    Args:
      html_directory: parent directory that stores the rebaselining results.
                      If None, a temp directory is created.

    Returns:
      the directory that stores the html related rebaselining results.
    """

    if not html_directory:
        html_directory = tempfile.mkdtemp()
    elif not os.path.exists(html_directory):
        os.mkdir(html_directory)

    html_directory = os.path.join(html_directory, 'rebaseline_html')
    logging.info('Html directory: "%s"', html_directory)

    if os.path.exists(html_directory):
        shutil.rmtree(html_directory, True)
        logging.info('Deleted file at html directory: "%s"', html_directory)

    if not os.path.exists(html_directory):
        os.mkdir(html_directory)
    return html_directory


def get_result_file_fullpath(html_directory, baseline_filename, platform,
                             result_type):
    """Get full path of the baseline result file.

    Args:
      html_directory: directory that stores the html related files.
      baseline_filename: name of the baseline file.
      platform: win, linux or mac
      result_type: type of the baseline result: '.txt', '.png'.

    Returns:
      Full path of the baseline file for rebaselining result comparison.
    """

    base, ext = os.path.splitext(baseline_filename)
    result_filename = '%s-%s-%s%s' % (base, platform, result_type, ext)
    fullpath = os.path.join(html_directory, result_filename)
    logging.debug('  Result file full path: "%s".', fullpath)
    return fullpath


class Rebaseliner(object):
    """Class to produce new baselines for a given platform."""

    REVISION_REGEX = r'<a href=\"(\d+)/\">'

    def __init__(self, platform, options):
        self._file_dir = path_utils.path_from_base('webkit', 'tools',
            'layout_tests')
        self._platform = platform
        self._options = options
        self._rebaselining_tests = []
        self._rebaselined_tests = []

        # Create tests and expectations helper which is used to:
        #   -. compile list of tests that need rebaselining.
        #   -. update the tests in test_expectations file after rebaseline
        #      is done.
        self._test_expectations = \
            test_expectations.TestExpectations(None,
                                               self._file_dir,
                                               platform,
                                               False,
                                               False)

        self._repo_type = self._get_repo_type()

    def run(self, backup):
        """Run rebaseline process."""

        log_dashed_string('Compiling rebaselining tests', self._platform)
        if not self._compile_rebaselining_tests():
            return True

        log_dashed_string('Downloading archive', self._platform)
        archive_file = self._download_buildbot_archive()
        logging.info('')
        if not archive_file:
            logging.error('No archive found.')
            return False

        log_dashed_string('Extracting and adding new baselines',
                          self._platform)
        if not self._extract_and_add_new_baselines(archive_file):
            return False

        log_dashed_string('Updating rebaselined tests in file',
                          self._platform)
        self._update_rebaselined_tests_in_file(backup)
        logging.info('')

        if len(self._rebaselining_tests) != len(self._rebaselined_tests):
            logging.warning('NOT ALL TESTS THAT NEED REBASELINING HAVE BEEN '
                            'REBASELINED.')
            logging.warning('  Total tests needing rebaselining: %d',
                            len(self._rebaselining_tests))
            logging.warning('  Total tests rebaselined: %d',
                            len(self._rebaselined_tests))
            return False

        logging.warning('All tests needing rebaselining were successfully '
                        'rebaselined.')

        return True

    def get_rebaselining_tests(self):
        return self._rebaselining_tests

    def _get_repo_type(self):
        """Get the repository type that client is using."""
        output, return_code = run_shell_with_return_code(['svn', 'info'],
                                                         False)
        if return_code == 0:
            return REPO_SVN

        return REPO_UNKNOWN

    def _compile_rebaselining_tests(self):
        """Compile list of tests that need rebaselining for the platform.

        Returns:
          List of tests that need rebaselining or
          None if there is no such test.
        """

        self._rebaselining_tests = \
            self._test_expectations.get_rebaselining_failures()
        if not self._rebaselining_tests:
            logging.warn('No tests found that need rebaselining.')
            return None

        logging.info('Total number of tests needing rebaselining '
                     'for "%s": "%d"', self._platform,
                     len(self._rebaselining_tests))

        test_no = 1
        for test in self._rebaselining_tests:
            logging.info('  %d: %s', test_no, test)
            test_no += 1

        return self._rebaselining_tests

    def _get_latest_revision(self, url):
        """Get the latest layout test revision number from buildbot.

        Args:
          url: Url to retrieve layout test revision numbers.

        Returns:
          latest revision or
          None on failure.
        """

        logging.debug('Url to retrieve revision: "%s"', url)

        f = urllib.urlopen(url)
        content = f.read()
        f.close()

        revisions = re.findall(self.REVISION_REGEX, content)
        if not revisions:
            logging.error('Failed to find revision, content: "%s"', content)
            return None

        revisions.sort(key=int)
        logging.info('Latest revision: "%s"', revisions[len(revisions) - 1])
        return revisions[len(revisions) - 1]

    def _get_archive_dir_name(self, platform, webkit_canary):
        """Get name of the layout test archive directory.

        Returns:
          Directory name or
          None on failure
        """

        if webkit_canary:
            platform += '-canary'

        if platform in ARCHIVE_DIR_NAME_DICT:
            return ARCHIVE_DIR_NAME_DICT[platform]
        else:
            logging.error('Cannot find platform key %s in archive '
                          'directory name dictionary', platform)
            return None

    def _get_archive_url(self):
        """Generate the url to download latest layout test archive.

        Returns:
          Url to download archive or
          None on failure
        """

        dir_name = self._get_archive_dir_name(self._platform,
                                              self._options.webkit_canary)
        if not dir_name:
            return None

        logging.debug('Buildbot platform dir name: "%s"', dir_name)

        url_base = '%s/%s/' % (self._options.archive_url, dir_name)
        latest_revision = self._get_latest_revision(url_base)
        if latest_revision is None or latest_revision <= 0:
            return None

        archive_url = ('%s%s/layout-test-results.zip' % (url_base,
                                                         latest_revision))
        logging.info('Archive url: "%s"', archive_url)
        return archive_url

    def _download_buildbot_archive(self):
        """Download layout test archive file from buildbot.

        Returns:
          True if download succeeded or
          False otherwise.
        """

        url = self._get_archive_url()
        if url is None:
            return None

        fn = urllib.urlretrieve(url)[0]
        logging.info('Archive downloaded and saved to file: "%s"', fn)
        return fn

    def _extract_and_add_new_baselines(self, archive_file):
        """Extract new baselines from archive and add them to SVN repository.

        Args:
          archive_file: full path to the archive file.

        Returns:
          List of tests that have been rebaselined or
          None on failure.
        """

        zip_file = zipfile.ZipFile(archive_file, 'r')
        zip_namelist = zip_file.namelist()

        logging.debug('zip file namelist:')
        for name in zip_namelist:
            logging.debug('  ' + name)

        platform = path_utils.platform_name(self._platform)
        logging.debug('Platform dir: "%s"', platform)

        test_no = 1
        self._rebaselined_tests = []
        for test in self._rebaselining_tests:
            logging.info('Test %d: %s', test_no, test)

            found = False
            svn_error = False
            test_basename = os.path.splitext(test)[0]
            for suffix in BASELINE_SUFFIXES:
                archive_test_name = ('layout-test-results/%s-actual%s' %
                                     (test_basename, suffix))
                logging.debug('  Archive test file name: "%s"',
                              archive_test_name)
                if not archive_test_name in zip_namelist:
                    logging.info('  %s file not in archive.', suffix)
                    continue

                found = True
                logging.info('  %s file found in archive.', suffix)

                # Extract new baseline from archive and save it to a temp file.
                data = zip_file.read(archive_test_name)
                temp_fd, temp_name = tempfile.mkstemp(suffix)
                f = os.fdopen(temp_fd, 'wb')
                f.write(data)
                f.close()

                expected_filename = '%s-expected%s' % (test_basename, suffix)
                expected_fullpath = os.path.join(
                    path_utils.chromium_baseline_path(platform),
                    expected_filename)
                expected_fullpath = os.path.normpath(expected_fullpath)
                logging.debug('  Expected file full path: "%s"',
                              expected_fullpath)

                # TODO(victorw): for now, the rebaselining tool checks whether
                # or not THIS baseline is duplicate and should be skipped.
                # We could improve the tool to check all baselines in upper
                # and lower
                # levels and remove all duplicated baselines.
                if self._is_dup_baseline(temp_name,
                                       expected_fullpath,
                                       test,
                                       suffix,
                                       self._platform):
                    os.remove(temp_name)
                    self._delete_baseline(expected_fullpath)
                    continue

                # Create the new baseline directory if it doesn't already
                # exist.
                path_utils.maybe_make_directory(
                    os.path.dirname(expected_fullpath))

                shutil.move(temp_name, expected_fullpath)

                if not self._svn_add(expected_fullpath):
                    svn_error = True
                elif suffix != '.checksum':
                    self._create_html_baseline_files(expected_fullpath)

            if not found:
                logging.warn('  No new baselines found in archive.')
            else:
                if svn_error:
                    logging.warn('  Failed to add baselines to SVN.')
                else:
                    logging.info('  Rebaseline succeeded.')
                    self._rebaselined_tests.append(test)

            test_no += 1

        zip_file.close()
        os.remove(archive_file)

        return self._rebaselined_tests

    def _is_dup_baseline(self, new_baseline, baseline_path, test, suffix,
                         platform):
        """Check whether a baseline is duplicate and can fallback to same
           baseline for another platform. For example, if a test has same
           baseline on linux and windows, then we only store windows
           baseline and linux baseline will fallback to the windows version.

        Args:
          expected_filename: baseline expectation file name.
          test: test name.
          suffix: file suffix of the expected results, including dot;
                  e.g. '.txt' or '.png'.
          platform: baseline platform 'mac', 'win' or 'linux'.

        Returns:
          True if the baseline is unnecessary.
          False otherwise.
        """
        test_filepath = os.path.join(path_utils.layout_tests_dir(), test)
        all_baselines = path_utils.expected_baselines(test_filepath,
                                                      suffix, platform, True)
        for (fallback_dir, fallback_file) in all_baselines:
            if fallback_dir and fallback_file:
                fallback_fullpath = os.path.normpath(
                    os.path.join(fallback_dir, fallback_file))
                if fallback_fullpath.lower() != baseline_path.lower():
                    if not self._diff_baselines(new_baseline,
                                                fallback_fullpath):
                        logging.info('  Found same baseline at %s',
                                     fallback_fullpath)
                        return True
                    else:
                        return False

        return False

    def _diff_baselines(self, file1, file2):
        """Check whether two baselines are different.

        Args:
          file1, file2: full paths of the baselines to compare.

        Returns:
          True if two files are different or have different extensions.
          False otherwise.
        """

        ext1 = os.path.splitext(file1)[1].upper()
        ext2 = os.path.splitext(file2)[1].upper()
        if ext1 != ext2:
            logging.warn('Files to compare have different ext. '
                         'File1: %s; File2: %s', file1, file2)
            return True

        if ext1 == '.PNG':
            return image_diff.ImageDiff(self._platform, '').diff_files(file1,
                                                                       file2)
        else:
            return text_diff.TestTextDiff(self._platform, '').diff_files(file1,
                                                                         file2)

    def _delete_baseline(self, filename):
        """Remove the file from repository and delete it from disk.

        Args:
          filename: full path of the file to delete.
        """

        if not filename or not os.path.isfile(filename):
            return

        if self._repo_type == REPO_SVN:
            parent_dir, basename = os.path.split(filename)
            original_dir = os.getcwd()
            os.chdir(parent_dir)
            run_shell(['svn', 'delete', '--force', basename], False)
            os.chdir(original_dir)
        else:
            os.remove(filename)

    def _update_rebaselined_tests_in_file(self, backup):
        """Update the rebaselined tests in test expectations file.

        Args:
          backup: if True, backup the original test expectations file.

        Returns:
          no
        """

        if self._rebaselined_tests:
            self._test_expectations.remove_platform_from_file(
                self._rebaselined_tests, self._platform, backup)
        else:
            logging.info('No test was rebaselined so nothing to remove.')

    def _svn_add(self, filename):
        """Add the file to SVN repository.

        Args:
          filename: full path of the file to add.

        Returns:
          True if the file already exists in SVN or is sucessfully added
               to SVN.
          False otherwise.
        """

        if not filename:
            return False

        parent_dir, basename = os.path.split(filename)
        if self._repo_type != REPO_SVN or parent_dir == filename:
            logging.info("No svn checkout found, skip svn add.")
            return True

        original_dir = os.getcwd()
        os.chdir(parent_dir)
        status_output = run_shell(['svn', 'status', basename], False)
        os.chdir(original_dir)
        output = status_output.upper()
        if output.startswith('A') or output.startswith('M'):
            logging.info('  File already added to SVN: "%s"', filename)
            return True

        if output.find('IS NOT A WORKING COPY') >= 0:
            logging.info('  File is not a working copy, add its parent: "%s"',
                         parent_dir)
            return self._svn_add(parent_dir)

        os.chdir(parent_dir)
        add_output = run_shell(['svn', 'add', basename], True)
        os.chdir(original_dir)
        output = add_output.upper().rstrip()
        if output.startswith('A') and output.find(basename.upper()) >= 0:
            logging.info('  Added new file: "%s"', filename)
            self._svn_prop_set(filename)
            return True

        if (not status_output) and (add_output.upper().find(
            'ALREADY UNDER VERSION CONTROL') >= 0):
            logging.info('  File already under SVN and has no change: "%s"',
                         filename)
            return True

        logging.warn('  Failed to add file to SVN: "%s"', filename)
        logging.warn('  Svn status output: "%s"', status_output)
        logging.warn('  Svn add output: "%s"', add_output)
        return False

    def _svn_prop_set(self, filename):
        """Set the baseline property

        Args:
          filename: full path of the file to add.

        Returns:
          True if the file already exists in SVN or is sucessfully added
               to SVN.
          False otherwise.
        """
        ext = os.path.splitext(filename)[1].upper()
        if ext != '.TXT' and ext != '.PNG' and ext != '.CHECKSUM':
            return

        parent_dir, basename = os.path.split(filename)
        original_dir = os.getcwd()
        os.chdir(parent_dir)
        if ext == '.PNG':
            cmd = ['svn', 'pset', 'svn:mime-type', 'image/png', basename]
        else:
            cmd = ['svn', 'pset', 'svn:eol-style', 'LF', basename]

        logging.debug('  Set svn prop: %s', ' '.join(cmd))
        run_shell(cmd, False)
        os.chdir(original_dir)

    def _create_html_baseline_files(self, baseline_fullpath):
        """Create baseline files (old, new and diff) in html directory.

           The files are used to compare the rebaselining results.

        Args:
          baseline_fullpath: full path of the expected baseline file.
        """

        if not baseline_fullpath or not os.path.exists(baseline_fullpath):
            return

        # Copy the new baseline to html directory for result comparison.
        baseline_filename = os.path.basename(baseline_fullpath)
        new_file = get_result_file_fullpath(self._options.html_directory,
                                            baseline_filename, self._platform,
                                            'new')
        shutil.copyfile(baseline_fullpath, new_file)
        logging.info('  Html: copied new baseline file from "%s" to "%s".',
                     baseline_fullpath, new_file)

        # Get the old baseline from SVN and save to the html directory.
        output = run_shell(['svn', 'cat', '-r', 'BASE', baseline_fullpath])
        if (not output) or (output.upper().rstrip().endswith(
            'NO SUCH FILE OR DIRECTORY')):
            logging.info('  No base file: "%s"', baseline_fullpath)
            return
        base_file = get_result_file_fullpath(self._options.html_directory,
                                             baseline_filename, self._platform,
                                             'old')
        f = open(base_file, 'wb')
        f.write(output)
        f.close()
        logging.info('  Html: created old baseline file: "%s".',
                     base_file)

        # Get the diff between old and new baselines and save to the html dir.
        if baseline_filename.upper().endswith('.TXT'):
            # If the user specified a custom diff command in their svn config
            # file, then it'll be used when we do svn diff, which we don't want
            # to happen since we want the unified diff.  Using --diff-cmd=diff
            # doesn't always work, since they can have another diff executable
            # in their path that gives different line endings.  So we use a
            # bogus temp directory as the config directory, which gets
            # around these problems.
            if sys.platform.startswith("win"):
                parent_dir = tempfile.gettempdir()
            else:
                parent_dir = sys.path[0]  # tempdir is not secure.
            bogus_dir = os.path.join(parent_dir, "temp_svn_config")
            logging.debug('  Html: temp config dir: "%s".', bogus_dir)
            if not os.path.exists(bogus_dir):
                os.mkdir(bogus_dir)
                delete_bogus_dir = True
            else:
                delete_bogus_dir = False

            output = run_shell(["svn", "diff", "--config-dir", bogus_dir,
                               baseline_fullpath])
            if output:
                diff_file = get_result_file_fullpath(
                    self._options.html_directory, baseline_filename,
                    self._platform, 'diff')
                f = open(diff_file, 'wb')
                f.write(output)
                f.close()
                logging.info('  Html: created baseline diff file: "%s".',
                             diff_file)

            if delete_bogus_dir:
                shutil.rmtree(bogus_dir, True)
                logging.debug('  Html: removed temp config dir: "%s".',
                              bogus_dir)


class HtmlGenerator(object):
    """Class to generate rebaselining result comparison html."""

    HTML_REBASELINE = ('<html>'
                       '<head>'
                       '<style>'
                       'body {font-family: sans-serif;}'
                       '.mainTable {background: #666666;}'
                       '.mainTable td , .mainTable th {background: white;}'
                       '.detail {margin-left: 10px; margin-top: 3px;}'
                       '</style>'
                       '<title>Rebaselining Result Comparison (%(time)s)'
                       '</title>'
                       '</head>'
                       '<body>'
                       '<h2>Rebaselining Result Comparison (%(time)s)</h2>'
                       '%(body)s'
                       '</body>'
                       '</html>')
    HTML_NO_REBASELINING_TESTS = (
        '<p>No tests found that need rebaselining.</p>')
    HTML_TABLE_TEST = ('<table class="mainTable" cellspacing=1 cellpadding=5>'
                       '%s</table><br>')
    HTML_TR_TEST = ('<tr>'
                    '<th style="background-color: #CDECDE; border-bottom: '
                    '1px solid black; font-size: 18pt; font-weight: bold" '
                    'colspan="5">'
                    '<a href="%s">%s</a>'
                    '</th>'
                    '</tr>')
    HTML_TEST_DETAIL = ('<div class="detail">'
                        '<tr>'
                        '<th width="100">Baseline</th>'
                        '<th width="100">Platform</th>'
                        '<th width="200">Old</th>'
                        '<th width="200">New</th>'
                        '<th width="150">Difference</th>'
                        '</tr>'
                        '%s'
                        '</div>')
    HTML_TD_NOLINK = '<td align=center><a>%s</a></td>'
    HTML_TD_LINK = '<td align=center><a href="%(uri)s">%(name)s</a></td>'
    HTML_TD_LINK_IMG = ('<td><a href="%(uri)s">'
                        '<img style="width: 200" src="%(uri)s" /></a></td>')
    HTML_TR = '<tr>%s</tr>'

    def __init__(self, options, platforms, rebaselining_tests):
        self._html_directory = options.html_directory
        self._platforms = platforms
        self._rebaselining_tests = rebaselining_tests
        self._html_file = os.path.join(options.html_directory,
                                       'rebaseline.html')

    def generate_html(self):
        """Generate html file for rebaselining result comparison."""

        logging.info('Generating html file')

        html_body = ''
        if not self._rebaselining_tests:
            html_body += self.HTML_NO_REBASELINING_TESTS
        else:
            tests = list(self._rebaselining_tests)
            tests.sort()

            test_no = 1
            for test in tests:
                logging.info('Test %d: %s', test_no, test)
                html_body += self._generate_html_for_one_test(test)

        html = self.HTML_REBASELINE % ({'time': time.asctime(),
                                        'body': html_body})
        logging.debug(html)

        f = open(self._html_file, 'w')
        f.write(html)
        f.close()

        logging.info('Baseline comparison html generated at "%s"',
                     self._html_file)

    def show_html(self):
        """Launch the rebaselining html in brwoser."""

        logging.info('Launching html: "%s"', self._html_file)

        html_uri = path_utils.filename_to_uri(self._html_file)
        webbrowser.open(html_uri, 1)

        logging.info('Html launched.')

    def _generate_baseline_links(self, test_basename, suffix, platform):
        """Generate links for baseline results (old, new and diff).

        Args:
          test_basename: base filename of the test
          suffix: baseline file suffixes: '.txt', '.png'
          platform: win, linux or mac

        Returns:
          html links for showing baseline results (old, new and diff)
        """

        baseline_filename = '%s-expected%s' % (test_basename, suffix)
        logging.debug('    baseline filename: "%s"', baseline_filename)

        new_file = get_result_file_fullpath(self._html_directory,
                                            baseline_filename, platform, 'new')
        logging.info('    New baseline file: "%s"', new_file)
        if not os.path.exists(new_file):
            logging.info('    No new baseline file: "%s"', new_file)
            return ''

        old_file = get_result_file_fullpath(self._html_directory,
                                            baseline_filename, platform, 'old')
        logging.info('    Old baseline file: "%s"', old_file)
        if suffix == '.png':
            html_td_link = self.HTML_TD_LINK_IMG
        else:
            html_td_link = self.HTML_TD_LINK

        links = ''
        if os.path.exists(old_file):
            links += html_td_link % {
                'uri': path_utils.filename_to_uri(old_file),
                'name': baseline_filename}
        else:
            logging.info('    No old baseline file: "%s"', old_file)
            links += self.HTML_TD_NOLINK % ''

        links += html_td_link % {'uri': path_utils.filename_to_uri(new_file),
                                 'name': baseline_filename}

        diff_file = get_result_file_fullpath(self._html_directory,
                                             baseline_filename, platform,
                                             'diff')
        logging.info('    Baseline diff file: "%s"', diff_file)
        if os.path.exists(diff_file):
            links += html_td_link % {'uri': path_utils.filename_to_uri(
                diff_file), 'name': 'Diff'}
        else:
            logging.info('    No baseline diff file: "%s"', diff_file)
            links += self.HTML_TD_NOLINK % ''

        return links

    def _generate_html_for_one_test(self, test):
        """Generate html for one rebaselining test.

        Args:
          test: layout test name

        Returns:
          html that compares baseline results for the test.
        """

        test_basename = os.path.basename(os.path.splitext(test)[0])
        logging.info('  basename: "%s"', test_basename)
        rows = []
        for suffix in BASELINE_SUFFIXES:
            if suffix == '.checksum':
                continue

            logging.info('  Checking %s files', suffix)
            for platform in self._platforms:
                links = self._generate_baseline_links(test_basename, suffix,
                    platform)
                if links:
                    row = self.HTML_TD_NOLINK % self._get_baseline_result_type(
                        suffix)
                    row += self.HTML_TD_NOLINK % platform
                    row += links
                    logging.debug('    html row: %s', row)

                    rows.append(self.HTML_TR % row)

        if rows:
            test_path = os.path.join(path_utils.layout_tests_dir(), test)
            html = self.HTML_TR_TEST % (path_utils.filename_to_uri(test_path),
                test)
            html += self.HTML_TEST_DETAIL % ' '.join(rows)

            logging.debug('    html for test: %s', html)
            return self.HTML_TABLE_TEST % html

        return ''

    def _get_baseline_result_type(self, suffix):
        """Name of the baseline result type."""

        if suffix == '.png':
            return 'Pixel'
        elif suffix == '.txt':
            return 'Render Tree'
        else:
            return 'Other'


def main():
    """Main function to produce new baselines."""

    option_parser = optparse.OptionParser()
    option_parser.add_option('-v', '--verbose',
                             action='store_true',
                             default=False,
                             help='include debug-level logging.')

    option_parser.add_option('-p', '--platforms',
                             default='mac,win,win-xp,win-vista,linux',
                             help=('Comma delimited list of platforms '
                                   'that need rebaselining.'))

    option_parser.add_option('-u', '--archive_url',
                             default=('http://build.chromium.org/buildbot/'
                                      'layout_test_results'),
                             help=('Url to find the layout test result archive'
                                   ' file.'))

    option_parser.add_option('-w', '--webkit_canary',
                             action='store_true',
                             default=False,
                             help=('If True, pull baselines from webkit.org '
                                   'canary bot.'))

    option_parser.add_option('-b', '--backup',
                             action='store_true',
                             default=False,
                             help=('Whether or not to backup the original test'
                                   ' expectations file after rebaseline.'))

    option_parser.add_option('-d', '--html_directory',
                             default='',
                             help=('The directory that stores the results for'
                                   ' rebaselining comparison.'))

    options = option_parser.parse_args()[0]

    # Set up our logging format.
    log_level = logging.INFO
    if options.verbose:
        log_level = logging.DEBUG
    logging.basicConfig(level=log_level,
                        format=('%(asctime)s %(filename)s:%(lineno)-3d '
                                '%(levelname)s %(message)s'),
                        datefmt='%y%m%d %H:%M:%S')

    # Verify 'platforms' option is valid
    if not options.platforms:
        logging.error('Invalid "platforms" option. --platforms must be '
                      'specified in order to rebaseline.')
        sys.exit(1)
    platforms = [p.strip().lower() for p in options.platforms.split(',')]
    for platform in platforms:
        if not platform in REBASELINE_PLATFORM_ORDER:
            logging.error('Invalid platform: "%s"' % (platform))
            sys.exit(1)

    # Adjust the platform order so rebaseline tool is running at the order of
    # 'mac', 'win' and 'linux'. This is in same order with layout test baseline
    # search paths. It simplifies how the rebaseline tool detects duplicate
    # baselines. Check _IsDupBaseline method for details.
    rebaseline_platforms = []
    for platform in REBASELINE_PLATFORM_ORDER:
        if platform in platforms:
            rebaseline_platforms.append(platform)

    options.html_directory = setup_html_directory(options.html_directory)

    rebaselining_tests = set()
    backup = options.backup
    for platform in rebaseline_platforms:
        rebaseliner = Rebaseliner(platform, options)

        logging.info('')
        log_dashed_string('Rebaseline started', platform)
        if rebaseliner.run(backup):
            # Only need to backup one original copy of test expectation file.
            backup = False
            log_dashed_string('Rebaseline done', platform)
        else:
            log_dashed_string('Rebaseline failed', platform, logging.ERROR)

        rebaselining_tests |= set(rebaseliner.get_rebaselining_tests())

    logging.info('')
    log_dashed_string('Rebaselining result comparison started', None)
    html_generator = HtmlGenerator(options,
                                   rebaseline_platforms,
                                   rebaselining_tests)
    html_generator.generate_html()
    html_generator.show_html()
    log_dashed_string('Rebaselining result comparison done', None)

    sys.exit(0)

if '__main__' == __name__:
    main()
