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

from __future__ import with_statement

import copy
import logging
import optparse
import re
import sys
import time

from webkitpy.common.checkout import scm
from webkitpy.common.system import zipfileset
from webkitpy.common.system import path
from webkitpy.common.system import urlfetcher
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.host import Host

from webkitpy.layout_tests.port.factory import PortFactory
from webkitpy.layout_tests import read_checksum_from_png
from webkitpy.layout_tests.models import test_expectations


_log = logging.getLogger(__name__)

BASELINE_SUFFIXES = ('.txt', '.png', '.checksum')

ARCHIVE_DIR_NAME_DICT = {
    'chromium-win-win7': 'Webkit_Win7',
    'chromium-win-vista': 'Webkit_Vista',
    'chromium-win-xp': 'Webkit_Win',
    'chromium-mac-leopard': 'Webkit_Mac10_5',
    'chromium-mac-snowleopard': 'Webkit_Mac10_6',
    'chromium-cg-mac-leopard': 'Webkit_Mac10_5__CG_',
    'chromium-cg-mac-snowleopard': 'Webkit_Mac10_6__CG_',
    'chromium-linux-x86': 'Webkit_Linux_32',
    'chromium-linux-x86_64': 'Webkit_Linux',
    'chromium-gpu-mac-snowleopard': 'Webkit_Mac10_6_-_GPU',
    'chromium-gpu-win-xp': 'Webkit_Win_-_GPU',
    'chromium-gpu-win-win7': 'Webkit_Win7_-_GPU',
    'chromium-gpu-linux-x86_64': 'Webkit_Linux_-_GPU',
    'chromium-gpu-linux-x86': 'Webkit_Linux_32_-_GPU',
}


def log_dashed_string(text, platform=None, logging_level=logging.DEBUG):
    """Log text message with dashes on both sides."""
    msg = text
    if platform:
        msg += ': ' + platform
    if len(msg) < 78:
        dashes = '-' * ((78 - len(msg)) / 2)
        msg = '%s %s %s' % (dashes, msg, dashes)
    _log.log(logging_level, msg)


def setup_html_directory(filesystem, parent_directory):
    """Setup the directory to store html results.

       All html related files are stored in the "rebaseline_html" subdirectory of
       the parent directory. The path to the created directory is returned.
    """

    if not parent_directory:
        parent_directory = str(filesystem.mkdtemp())
    else:
        filesystem.maybe_make_directory(parent_directory)

    html_directory = filesystem.join(parent_directory, 'rebaseline_html')
    _log.debug('Html directory: "%s"', html_directory)

    if filesystem.exists(html_directory):
        filesystem.rmtree(html_directory)
        _log.debug('Deleted html directory: "%s"', html_directory)

    filesystem.maybe_make_directory(html_directory)
    return html_directory


def get_result_file_fullpath(filesystem, html_directory, baseline_filename, platform,
                             result_type):
    """Get full path of the baseline result file.

    Args:
      filesystem: wrapper object
      html_directory: directory that stores the html related files.
      baseline_filename: name of the baseline file.
      platform: win, linux or mac
      result_type: type of the baseline result: '.txt', '.png'.

    Returns:
      Full path of the baseline file for rebaselining result comparison.
    """

    base, ext = filesystem.splitext(baseline_filename)
    result_filename = '%s-%s-%s%s' % (base, platform, result_type, ext)
    fullpath = filesystem.join(html_directory, result_filename)
    _log.debug('  Result file full path: "%s".', fullpath)
    return fullpath


class Rebaseliner(object):
    """Class to produce new baselines for a given platform."""

    REVISION_REGEX = r'<a href=\"(\d+)/\">'

    def __init__(self, host, running_port, target_port, platform, options, url_fetcher, zip_factory, logged_before=False):
        """
        Args:
            running_port: the Port the script is running on.
            target_port: the Port the script uses to find port-specific
                configuration information like the test_expectations.txt
                file location and the list of test platforms.
            platform: the test platform to rebaseline
            options: the command-line options object.
            url_fetcher: object that can fetch objects from URLs
            zip_factory: optional object that can fetch zip files from URLs
            scm: scm object for adding new baselines
            logged_before: whether the previous running port logged anything.
        """
        self._platform = platform
        self._options = options
        self._port = running_port
        self._filesystem = host.filesystem
        self._target_port = target_port

        self._rebaseline_port = host.port_factory.get(platform, options)
        self._rebaselining_tests = set()
        self._rebaselined_tests = []
        self._logged_before = logged_before
        self.did_log = False

        # Create tests and expectations helper which is used to:
        #   -. compile list of tests that need rebaselining.
        #   -. update the tests in test_expectations file after rebaseline
        #      is done.
        expectations_str = self._rebaseline_port.test_expectations()
        self._test_expectations = test_expectations.TestExpectations(
            self._rebaseline_port, None, expectations_str, self._rebaseline_port.test_configuration(), False)
        self._url_fetcher = url_fetcher
        self._zip_factory = zip_factory
        self._scm = host.scm()

    def run(self):
        """Run rebaseline process."""

        log_dashed_string('Compiling rebaselining tests', self._platform, logging.DEBUG)
        if not self._compile_rebaselining_tests():
            return False
        if not self._rebaselining_tests:
            return True

        self.did_log = True
        log_dashed_string('Downloading archive', self._platform, logging.DEBUG)
        archive_file = self._download_buildbot_archive()
        _log.debug('')
        if not archive_file:
            _log.error('No archive found.')
            return False

        log_dashed_string('Extracting and adding new baselines', self._platform, logging.DEBUG)
        self._extract_and_add_new_baselines(archive_file)
        archive_file.close()

        log_dashed_string('Updating rebaselined tests in file', self._platform)

        if len(self._rebaselining_tests) != len(self._rebaselined_tests):
            _log.debug('')
            _log.debug('NOT ALL TESTS WERE REBASELINED.')
            _log.debug('  Number marked for rebaselining: %d', len(self._rebaselining_tests))
            _log.debug('  Number actually rebaselined: %d', len(self._rebaselined_tests))
            _log.info('')
            return False

        _log.debug('  All tests needing rebaselining were successfully rebaselined.')
        _log.info('')
        return True

    def remove_rebaselining_expectations(self, tests, backup):
        """if backup is True, we backup the original test expectations file."""
        new_expectations = self._test_expectations.remove_rebaselined_tests(tests)
        path = self._target_port.path_to_test_expectations_file()
        if backup:
            date_suffix = time.strftime('%Y%m%d%H%M%S', time.localtime(time.time()))
            backup_file = '%s.orig.%s' % (path, date_suffix)
            if self._filesystem.exists(backup_file):
                self._filesystem.remove(backup_file)
            _log.debug('Saving original file to "%s"', backup_file)
            self._filesystem.move(path, backup_file)

        self._filesystem.write_text_file(path, new_expectations)
        # self._scm.add(path)

    def get_rebaselined_tests(self):
        return self._rebaselined_tests

    def _compile_rebaselining_tests(self):
        """Compile list of tests that need rebaselining for the platform.

        Returns:
          False if reftests are wrongly marked as 'needs rebaselining' or True
        """

        self._rebaselining_tests = self._test_expectations.get_rebaselining_failures()
        if not self._rebaselining_tests:
            _log.info('%s: No tests to rebaseline.', self._platform)
            return True

        fs = self._target_port._filesystem
        for test in self._rebaselining_tests:
            if self._target_port.is_reftest(test):
                _log.error('%s seems to be a reftest. We can not rebase for reftests.', test)
                self._rebaselining_tests = set()
                return False

        if not self._logged_before:
            _log.info('')
        _log.info('%s: Rebaselining %d tests:', self._platform, len(self._rebaselining_tests))
        test_no = 1
        for test in self._rebaselining_tests:
            _log.debug('  %d: %s', test_no, test)
            test_no += 1

        return True

    def _get_latest_revision(self, url):
        """Get the latest layout test revision number from buildbot.

        Args:
          url: Url to retrieve layout test revision numbers.

        Returns:
          latest revision or
          None on failure.
        """

        _log.debug('Url to retrieve revision: "%s"', url)

        content = self._url_fetcher.fetch(url)

        revisions = re.findall(self.REVISION_REGEX, content)
        if not revisions:
            _log.error('Failed to find revision, content: "%s"', content)
            return None

        revisions.sort(key=int)
        _log.debug('  Latest revision: %s', revisions[len(revisions) - 1])
        return revisions[len(revisions) - 1]

    def _get_archive_dir_name(self, platform):
        """Get name of the layout test archive directory.

        Returns:
          Directory name or
          None on failure
        """

        if platform in ARCHIVE_DIR_NAME_DICT:
            return ARCHIVE_DIR_NAME_DICT[platform]
        else:
            _log.error('Cannot find platform key %s in archive '
                       'directory name dictionary', platform)
            return None

    def _get_archive_url(self):
        """Generate the url to download latest layout test archive.

        Returns:
          Url to download archive or
          None on failure
        """

        if self._options.force_archive_url:
            return self._options.force_archive_url

        dir_name = self._get_archive_dir_name(self._platform)
        if not dir_name:
            return None

        _log.debug('Buildbot platform dir name: "%s"', dir_name)

        url_base = '%s/%s/' % (self._options.archive_url, dir_name)
        latest_revision = self._get_latest_revision(url_base)
        if latest_revision is None or latest_revision <= 0:
            return None
        archive_url = '%s%s/layout-test-results.zip' % (url_base, latest_revision)
        _log.info('  Using %s', archive_url)
        return archive_url

    def _download_buildbot_archive(self):
        """Download layout test archive file from buildbot and return a handle to it."""
        url = self._get_archive_url()
        if url is None:
            return None

        archive_file = zipfileset.ZipFileSet(url, filesystem=self._filesystem,
                                             zip_factory=self._zip_factory)
        _log.debug('Archive downloaded')
        return archive_file

    def _extract_and_add_new_baselines(self, zip_file):
        """Extract new baselines from the zip file and add them to SVN repository.

        Returns:
          List of tests that have been rebaselined or None on failure."""
        zip_namelist = zip_file.namelist()

        _log.debug('zip file namelist:')
        for name in zip_namelist:
            _log.debug('  ' + name)

        _log.debug('Platform dir: "%s"', self._platform)

        self._rebaselined_tests = []
        for test_no, test in enumerate(self._rebaselining_tests):
            _log.debug('Test %d: %s', test_no + 1, test)
            self._extract_and_add_new_baseline(test, zip_file)

    def _extract_and_add_new_baseline(self, test, zip_file):
        found = False
        scm_error = False
        test_basename = self._filesystem.splitext(test)[0]
        for suffix in BASELINE_SUFFIXES:
            archive_test_name = 'layout-test-results/%s-actual%s' % (test_basename, suffix)
            _log.debug('  Archive test file name: "%s"', archive_test_name)
            if not archive_test_name in zip_file.namelist():
                _log.debug('  %s file not in archive.', suffix)
                continue

            found = True
            _log.debug('  %s file found in archive.', suffix)

            temp_name = self._extract_from_zip_to_tempfile(zip_file, archive_test_name)

            expected_filename = '%s-expected%s' % (test_basename, suffix)
            expected_fullpath = self._filesystem.join(
                self._rebaseline_port.baseline_path(), expected_filename)
            expected_fullpath = self._filesystem.normpath(expected_fullpath)
            _log.debug('  Expected file full path: "%s"', expected_fullpath)

            relpath = self._filesystem.relpath(expected_fullpath, self._target_port.layout_tests_dir())

            # TODO(victorw): for now, the rebaselining tool checks whether
            # or not THIS baseline is duplicate and should be skipped.
            # We could improve the tool to check all baselines in upper
            # and lower levels and remove all duplicated baselines.
            if self._is_dup_baseline(temp_name, expected_fullpath, test, suffix, self._platform):
                self._filesystem.remove(temp_name)
                if self._filesystem.exists(expected_fullpath):
                    _log.info('  Removing %s' % relpath)
                    self._delete_baseline(expected_fullpath)
                _log.debug('  %s is a duplicate' % relpath)

                # FIXME: We consider a duplicate baseline a success in the normal case.
                # FIXME: This may not be what you want sometimes; should this be
                # FIXME: controllable?
                self._rebaselined_tests.append(test)
                continue

            if suffix == '.checksum' and self._png_has_same_checksum(temp_name, test, expected_fullpath):
                self._filesystem.remove(temp_name)
                # If an old checksum exists, delete it.
                self._delete_baseline(expected_fullpath)
                continue

            self._filesystem.maybe_make_directory(self._filesystem.dirname(expected_fullpath))
            self._filesystem.move(temp_name, expected_fullpath)

            path_from_base = self._filesystem.relpath(expected_fullpath)
            if self._scm.exists(path_from_base):
                _log.info('  Updating %s' % relpath)
            else:
                _log.info('  Adding %s' % relpath)

            if self._scm.add(expected_fullpath, return_exit_code=True):
                # FIXME: print detailed diagnose messages
                scm_error = True
            elif suffix != '.checksum':
                self._create_html_baseline_files(expected_fullpath)

        if not found:
            _log.warn('No results in archive for %s' % test)
        elif scm_error:
            _log.warn('Failed to add baselines to your repository.')
        else:
            _log.debug('  Rebaseline succeeded.')
            self._rebaselined_tests.append(test)

    def _extract_from_zip_to_tempfile(self, zip_file, filename):
        """Extracts |filename| from |zip_file|, a ZipFileSet. Returns the full
           path name to the extracted file."""
        data = zip_file.read(filename)
        suffix = self._filesystem.splitext(filename)[1]
        tempfile, temp_name = self._filesystem.open_binary_tempfile(suffix)
        tempfile.write(data)
        tempfile.close()
        return temp_name

    def _png_has_same_checksum(self, checksum_path, test, checksum_expected_fullpath):
        """Returns True if the fallback png for |checksum_expected_fullpath|
        contains the same checksum."""
        fs = self._filesystem
        png_fullpath = self._first_fallback_png_for_test(test)

        if not fs.exists(png_fullpath):
            _log.error('  Checksum without png file found! Expected %s to exist.' % png_fullpath)
            return False

        with fs.open_binary_file_for_reading(png_fullpath) as filehandle:
            checksum_in_png = read_checksum_from_png.read_checksum(filehandle)
            checksum_in_text_file = fs.read_text_file(checksum_path)
            if checksum_in_png and checksum_in_png != checksum_in_text_file:
                _log.error("  checksum in %s and %s don't match!  Continuing"
                           " to copy but please investigate." % (
                           checksum_expected_fullpath, png_fullpath))
            return checksum_in_text_file == checksum_in_png

    def _first_fallback_png_for_test(self, test):
        all_baselines = self._rebaseline_port.expected_baselines(test, '.png', True)
        return self._filesystem.join(all_baselines[0][0], all_baselines[0][1])

    def _is_dup_baseline(self, new_baseline, baseline_path, test, suffix, platform):
        """Check whether a baseline is duplicate and can fallback to same
           baseline for another platform. For example, if a test has same
           baseline on linux and windows, then we only store windows
           baseline and linux baseline will fallback to the windows version.

        Args:
          new_baseline: temp filename containing the new baseline results
          baseline_path: baseline expectation file name.
          test: test name.
          suffix: file suffix of the expected results, including dot;
                  e.g. '.txt' or '.png'.
          platform: baseline platform 'mac', 'win' or 'linux'.

        Returns:
          True if the baseline is unnecessary.
          False otherwise.
        """
        all_baselines = self._rebaseline_port.expected_baselines(test, suffix, True)

        for fallback_dir, fallback_file in all_baselines:
            if not fallback_dir or not fallback_file:
                continue

            fallback_fullpath = self._filesystem.normpath(
                self._filesystem.join(fallback_dir, fallback_file))
            if fallback_fullpath.lower() == baseline_path.lower():
                continue
            fallback_dir_relpath = self._filesystem.relpath(fallback_dir, self._target_port.layout_tests_dir())
            if fallback_dir_relpath == '':
                fallback_dir_relpath = '<generic>'

            new_output = self._filesystem.read_binary_file(new_baseline)
            fallback_output = self._filesystem.read_binary_file(fallback_fullpath)
            is_image = baseline_path.lower().endswith('.png')
            if not self._diff_baselines(new_output, fallback_output, is_image):
                _log.info('  Skipping %s (matches %s)', test, fallback_dir_relpath)
                return True
            return False

        return False

    def _diff_baselines(self, output1, output2, is_image):
        """Check whether two baselines are different.

        Args:
          output1, output2: contents of the baselines to compare.

        Returns:
          True if two files are different or have different extensions.
          False otherwise.
        """

        if is_image:
            return self._port.diff_image(output1, output2)[0]

        return self._port.compare_text(output1, output2)

    def _delete_baseline(self, filename):
        """Remove the file from repository and delete it from disk.

        Args:
          filename: full path of the file to delete.
        """

        if not filename or not self._filesystem.isfile(filename):
            return
        self._scm.delete(filename)

    def _create_html_baseline_files(self, baseline_fullpath):
        """Create baseline files (old, new and diff) in html directory.

           The files are used to compare the rebaselining results.

        Args:
          baseline_fullpath: full path of the expected baseline file.
        """

        baseline_relpath = self._filesystem.relpath(baseline_fullpath)
        _log.debug('  Html: create baselines for "%s"', baseline_relpath)

        if (not baseline_fullpath
            or not self._filesystem.exists(baseline_fullpath)):
            _log.debug('  Html: Does not exist: "%s"', baseline_fullpath)
            return

        if not self._scm.exists(baseline_relpath):
            _log.debug('  Html: Does not exist in scm: "%s"', baseline_relpath)
            return

        # Copy the new baseline to html directory for result comparison.
        baseline_filename = self._filesystem.basename(baseline_fullpath)
        new_file = get_result_file_fullpath(self._filesystem, self._options.html_directory,
                                            baseline_filename, self._platform, 'new')
        self._filesystem.copyfile(baseline_fullpath, new_file)
        _log.debug('  Html: copied new baseline file from "%s" to "%s".',
                  baseline_fullpath, new_file)

        # Get the old baseline from the repository and save to the html directory.
        try:
            output = self._scm.show_head(baseline_relpath)
        except ScriptError, e:
            _log.warning(e)
            output = ""

        if (not output) or (output.upper().rstrip().endswith('NO SUCH FILE OR DIRECTORY')):
            _log.warning('  No base file: "%s"', baseline_fullpath)
            return
        base_file = get_result_file_fullpath(self._filesystem, self._options.html_directory,
                                             baseline_filename, self._platform, 'old')
        if base_file.upper().endswith('.PNG'):
            self._filesystem.write_binary_file(base_file, output)
        else:
            self._filesystem.write_text_file(base_file, output)
        _log.debug('  Html: created old baseline file: "%s".', base_file)

        # Get the diff between old and new baselines and save to the html dir.
        diff_file = get_result_file_fullpath(self._filesystem,
                                             self._options.html_directory,
                                             baseline_filename,
                                             self._platform, 'diff')
        has_diff = False
        if baseline_filename.upper().endswith('.TXT'):
            output = self._scm.diff_for_file(baseline_relpath, log=_log)
            if output:
                self._filesystem.write_text_file(diff_file, output)
                has_diff = True
        elif baseline_filename.upper().endswith('.PNG'):
            old_file = get_result_file_fullpath(self._filesystem,
                                                self._options.html_directory,
                                                baseline_filename,
                                                self._platform, 'old')
            new_file = get_result_file_fullpath(self._filesystem,
                                                self._options.html_directory,
                                                baseline_filename,
                                                self._platform, 'new')
            _log.debug(' Html: diffing "%s" and "%s"', old_file, new_file)
            old_output = self._filesystem.read_binary_file(old_file)
            new_output = self._filesystem.read_binary_file(new_file)
            image_diff = self._port.diff_image(old_output, new_output)[0]
            self._filesystem.write_binary_file(diff_file, image_diff)

        if has_diff:
            _log.debug('  Html: created baseline diff file: "%s".', diff_file)


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

    def __init__(self, port, target_port, options, platforms, rebaselining_tests):
        self._html_directory = options.html_directory
        self._port = port
        self._target_port = target_port
        self._options = options
        self._platforms = platforms
        self._rebaselining_tests = rebaselining_tests
        self._filesystem = port._filesystem
        self._html_file = self._filesystem.join(options.html_directory,
                                                'rebaseline.html')

    def abspath_to_uri(self, filename):
        """Converts an absolute path to a file: URI."""
        return path.abspath_to_uri(filename, self._port._executive)

    def generate_html(self):
        """Generate html file for rebaselining result comparison."""

        _log.debug('Generating html file')

        html_body = ''
        if not self._rebaselining_tests:
            html_body += self.HTML_NO_REBASELINING_TESTS
        else:
            tests = list(self._rebaselining_tests)
            tests.sort()

            test_no = 1
            for test in tests:
                _log.debug('Test %d: %s', test_no, test)
                html_body += self._generate_html_for_one_test(test)

        html = self.HTML_REBASELINE % ({'time': time.asctime(),
                                        'body': html_body})
        _log.debug(html)

        self._filesystem.write_text_file(self._html_file, html)
        _log.debug('Baseline comparison html generated at "%s"', self._html_file)

    def show_html(self):
        """Launch the rebaselining html in brwoser."""

        _log.debug('Launching html: "%s"', self._html_file)
        self._port._user.open_url(self._html_file)
        _log.debug('Html launched.')

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
        _log.debug('    baseline filename: "%s"', baseline_filename)

        new_file = get_result_file_fullpath(self._filesystem, self._html_directory,
                                            baseline_filename, platform, 'new')
        _log.debug('    New baseline file: "%s"', new_file)
        if not self._filesystem.exists(new_file):
            _log.debug('    No new baseline file: "%s"', new_file)
            return ''

        old_file = get_result_file_fullpath(self._filesystem, self._html_directory,
                                            baseline_filename, platform, 'old')
        _log.debug('    Old baseline file: "%s"', old_file)
        if suffix == '.png':
            html_td_link = self.HTML_TD_LINK_IMG
        else:
            html_td_link = self.HTML_TD_LINK

        links = ''
        if self._filesystem.exists(old_file):
            links += html_td_link % {
                'uri': self.abspath_to_uri(old_file),
                'name': baseline_filename}
        else:
            _log.debug('    No old baseline file: "%s"', old_file)
            links += self.HTML_TD_NOLINK % ''

        links += html_td_link % {'uri': self.abspath_to_uri(new_file),
                                 'name': baseline_filename}

        diff_file = get_result_file_fullpath(self._filesystem, self._html_directory,
                                             baseline_filename, platform, 'diff')
        _log.debug('    Baseline diff file: "%s"', diff_file)
        if self._filesystem.exists(diff_file):
            links += html_td_link % {'uri': self.abspath_to_uri(diff_file),
                                     'name': 'Diff'}
        else:
            _log.debug('    No baseline diff file: "%s"', diff_file)
            links += self.HTML_TD_NOLINK % ''

        return links

    def _generate_html_for_one_test(self, test):
        """Generate html for one rebaselining test.

        Args:
          test: layout test name

        Returns:
          html that compares baseline results for the test.
        """

        test_basename = self._filesystem.basename(self._filesystem.splitext(test)[0])
        _log.debug('  basename: "%s"', test_basename)
        rows = []
        for suffix in BASELINE_SUFFIXES:
            if suffix == '.checksum':
                continue

            _log.debug('  Checking %s files', suffix)
            for platform in self._platforms:
                links = self._generate_baseline_links(test_basename, suffix, platform)
                if links:
                    row = self.HTML_TD_NOLINK % self._get_baseline_result_type(suffix)
                    row += self.HTML_TD_NOLINK % platform
                    row += links
                    _log.debug('    html row: %s', row)

                    rows.append(self.HTML_TR % row)

        if rows:
            test_path = self._filesystem.join(self._target_port.layout_tests_dir(), test)
            html = self.HTML_TR_TEST % (self.abspath_to_uri(test_path), test)
            html += self.HTML_TEST_DETAIL % ' '.join(rows)

            _log.debug('    html for test: %s', html)
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


def get_host_port_object(port_factory, options):
    """Return a port object for the platform we're running on."""
    # We want the ImageDiff logic to match that of the chromium bots, so we
    # force the use of a Chromium port.  We will look for either Debug or
    # Release versions.
    options.configuration = "Release"
    options.chromium = True
    port_obj = port_factory.get(options=options)
    if not port_obj.check_image_diff(override_step=None, logging=False):
        _log.debug('No release version of the image diff binary was found.')
        options.configuration = "Debug"
        port_obj = port_factory.get(options=options)
        if not port_obj.check_image_diff(override_step=None, logging=False):
            _log.error('No version of image diff was found. Check your build.')
            return None
        else:
            _log.debug('Found the debug version of the image diff binary.')
    else:
        _log.debug('Found the release version of the image diff binary.')
    return port_obj


def parse_options(args):
    """Parse options and return a pair of host options and target options."""
    option_parser = optparse.OptionParser()
    option_parser.add_option('-v', '--verbose',
                             action='store_true',
                             default=False,
                             help='include debug-level logging.')

    option_parser.add_option('-q', '--quiet',
                             action='store_true',
                             help='Suppress result HTML viewing')

    option_parser.add_option('-p', '--platforms',
                             default=None,
                             help=('Comma delimited list of platforms '
                                   'that need rebaselining.'))

    option_parser.add_option('-u', '--archive_url',
                             default=('http://build.chromium.org/f/chromium/'
                                      'layout_test_results'),
                             help=('Url to find the layout test result archive'
                                   ' file.'))
    option_parser.add_option('-U', '--force_archive_url',
                             help=('Url of result zip file. This option is for debugging '
                                   'purposes'))

    option_parser.add_option('-b', '--backup',
                             action='store_true',
                             default=False,
                             help=('Whether or not to backup the original test'
                                   ' expectations file after rebaseline.'))

    option_parser.add_option('-d', '--html_directory',
                             default='',
                             help=('The directory that stores the results for '
                                   'rebaselining comparison.'))

    option_parser.add_option('', '--use_drt',
                             action='store_true',
                             default=False,
                             help=('Use ImageDiff from DumpRenderTree instead '
                                   'of image_diff for pixel tests.'))

    option_parser.add_option('-w', '--webkit_canary',
                             action='store_true',
                             default=False,
                             help=('DEPRECATED. This flag no longer has any effect.'
                                   '  The canaries are always used.'))

    option_parser.add_option('', '--target-platform',
                             default='chromium',
                             help=('The target platform to rebaseline '
                                   '("mac", "chromium", "qt", etc.). Defaults '
                                   'to "chromium".'))

    options = option_parser.parse_args(args)[0]
    if options.webkit_canary:
        print "-w/--webkit-canary is no longer necessary, ignoring."

    target_options = copy.copy(options)
    if options.target_platform == 'chromium':
        target_options.chromium = True
    options.tolerance = 0

    return (options, target_options)


class DebugLogHandler(logging.Handler):
    num_failures = 0

    def __init__(self):
        logging.Handler.__init__(self)
        self.formatter = logging.Formatter(fmt=('%(asctime)s %(filename)s:%(lineno)-3d '
                                                '%(levelname)s %(message)s'))
        self.setFormatter(self.formatter)

    def emit(self, record):
        if record.levelno > logging.INFO:
            self.num_failures += 1
        print self.format(record)


class NormalLogHandler(logging.Handler):
    last_levelno = None
    num_failures = 0

    def emit(self, record):
        if record.levelno > logging.INFO:
            self.num_failures += 1
        if self.last_levelno != record.levelno:
            print
            self.last_levelno = record.levelno
        prefix = ''
        msg = record.getMessage()
        if record.levelno > logging.INFO and msg:
            prefix = '%s: ' % record.levelname
        print '%s%s' % (prefix, msg)


def main(args):
    """Bootstrap function that sets up the object references we need and calls real_main()."""
    options, target_options = parse_options(args)

    logger = logging.getLogger()
    logger.setLevel(logging.INFO)
    if options.verbose:
        log_level = logging.DEBUG
        log_handler = DebugLogHandler()
    else:
        log_level = logging.INFO
        log_handler = NormalLogHandler()

    logger = logging.getLogger()
    logger.setLevel(log_level)
    logger.addHandler(log_handler)

    host = Host()
    host._initialize_scm()
    target_port_obj = host.port_factory.get(None, target_options)
    host_port_obj = get_host_port_object(host.port_factory, options)
    if not host_port_obj or not target_port_obj:
        return 1

    url_fetcher = urlfetcher.UrlFetcher(host.filesystem)

    # We use the default zip factory method.
    zip_factory = None

    # FIXME: SCM module doesn't handle paths that aren't relative to the checkout_root consistently.
    host_port_obj._filesystem.chdir(host.scm().checkout_root)

    ret_code = real_main(host, options, target_options, host_port_obj, target_port_obj, url_fetcher, zip_factory)
    if not ret_code and log_handler.num_failures:
        ret_code = 1
    print ''
    if ret_code:
        print 'Rebaselining failed.'
    else:
        print 'Rebaselining succeeded.'
    return ret_code


def real_main(host, options, target_options, host_port_obj, target_port_obj, url_fetcher, zip_factory):
    """Main function to produce new baselines. The Rebaseliner object uses two
    different Port objects - one to represent the machine the object is running
    on, and one to represent the port whose expectations are being updated.
    E.g., you can run the script on a mac and rebaseline the 'win' port.

    Args:
        host: Host object
        options: command-line argument used for the host_port_obj (see below)
        target_options: command_line argument used for the target_port_obj.
            This object may have slightly different values than |options|.
        host_port_obj: a Port object for the platform the script is running
            on. This is used to produce image and text diffs, mostly, and
            is usually acquired from get_host_port_obj().
        target_port_obj: a Port obj representing the port getting rebaselined.
            This is used to find the expectations file, the baseline paths,
            etc.
        url_fetcher: object used to download the build archives from the bots
        zip_factory: factory function used to create zip file objects for
            the archives.
    """
    options.html_directory = setup_html_directory(host_port_obj._filesystem, options.html_directory)
    all_platforms = target_port_obj.all_baseline_variants()
    if options.platforms:
        bail = False
        for platform in options.platforms:
            if not platform in all_platforms:
                _log.error('Invalid platform: "%s"' % (platform))
                bail = True
        if bail:
            return 1
        rebaseline_platforms = options.platforms
    else:
        rebaseline_platforms = all_platforms

    # FIXME: These log messages will be wrong if ports store baselines outside
    # of layout_tests_dir(), but the code should work correctly.
    layout_tests_dir = target_port_obj.layout_tests_dir()
    expectations_path = target_port_obj.path_to_test_expectations_file()
    _log.info('Using %s' % layout_tests_dir)
    _log.info('  and %s' % expectations_path)

    rebaselined_tests = set()
    logged_before = False
    for platform in rebaseline_platforms:
        rebaseliner = Rebaseliner(host, host_port_obj, target_port_obj,
                                  platform, options, url_fetcher, zip_factory,
                                  logged_before)

        _log.debug('')
        log_dashed_string('Rebaseline started', platform)
        if rebaseliner.run():
            log_dashed_string('Rebaseline done', platform)
        else:
            log_dashed_string('Rebaseline failed', platform)

        rebaselined_tests |= set(rebaseliner.get_rebaselined_tests())
        logged_before = rebaseliner.did_log

    if rebaselined_tests:
        rebaseliner.remove_rebaselining_expectations(rebaselined_tests,
                                                     options.backup)

    _log.debug('')
    log_dashed_string('Rebaselining result comparison started')
    html_generator = HtmlGenerator(host_port_obj,
                                   target_port_obj,
                                   options,
                                   rebaseline_platforms,
                                   rebaselined_tests)
    html_generator.generate_html()
    if not options.quiet:
        html_generator.show_html()
    log_dashed_string('Rebaselining result comparison done')

    return 0


if '__main__' == __name__:
    sys.exit(main(sys.argv[1:]))
