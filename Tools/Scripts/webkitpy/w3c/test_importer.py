#!/usr/bin/env python

# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
# Copyright (C) 2015 Canon Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

"""
 This script imports W3C tests into WebKit, either from a local folder or by cloning W3C CSS and WPT repositories.

 This script will import the tests into WebKit following these rules:

    - All tests are by default imported into LayoutTests/imported/w3c

    - Tests will be imported into a directory tree that mirrors WPT repository in LayoutTests/imported/w3c/web-platform-tests.

    - By default, only reftests and jstest are imported. This can be overridden with a -a or --all
      argument

    - Also by default, if test files by the same name already exist in the destination directory,
      they are overwritten with the idea that running this script would refresh files periodically.
      This can also be overridden by a -n or --no-overwrite flag

    - If no import_directory is provided, the script will download the tests from the W3C github repositories.
      The selection of tests and folders to import will be based on the following files:
         1. LayoutTests/imported/w3c/resources/TestRepositories lists the repositories to clone, the corresponding revision to checkout and the infrastructure folders that need to be imported/skipped.
         2. LayoutTests/imported/w3c/resources/ImportExpectations list the test suites or tests to NOT import.

    - All files are converted to work in WebKit:
         1. Paths to testharness.js files are modified to point to Webkit's copy of them in
            LayoutTests/resources, using the correct relative path from the new location.
            This is applied to CSS tests but not to WPT tests.
         2. All CSS properties requiring the -webkit-vendor prefix are prefixed - this current
            list of what needs prefixes is read from Source/WebCore/CSS/CSSProperties.in
         3. Each reftest has its own copy of its reference file following the naming conventions
            new-run-webkit-tests expects
         4. If a reference files lives outside the directory of the test that uses it, it is checked
            for paths to support files as it will be imported into a different relative position to the
            test file (in the same directory)

     - Upon completion, script outputs the total number tests imported, broken down by test type

     - Also upon completion, each directory where files are imported will have w3c-import.log written
       with a timestamp, the list of CSS properties used that require prefixes, the list of imported files,
       and guidance for future test modification and maintenance.

     - On subsequent imports, this file is read to determine if files have been removed in the newer changesets.
       The script removes these files accordingly.
"""

import argparse
import json
import logging
import mimetypes

from webkitpy.common.host import Host
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.w3c.common import WPT_GH_URL, WPTPaths
from webkitpy.w3c.test_parser import TestParser
from webkitpy.w3c.test_converter import convert_for_webkit
from webkitpy.w3c.test_downloader import TestDownloader

CHANGESET_NOT_AVAILABLE = 'Not Available'

_log = logging.getLogger(__name__)


def main(_argv, _stdout, _stderr):
    options, test_paths = parse_args(_argv)

    configure_logging()

    test_importer = TestImporter(Host(), test_paths, options)
    test_importer.do_import()


def configure_logging():
    class LogHandler(logging.StreamHandler):

        def format(self, record):
            if record.levelno > logging.INFO:
                return "%s: %s" % (record.levelname, record.getMessage())
            return record.getMessage()

    logger = logging.getLogger()
    logger.setLevel(logging.INFO)
    handler = LogHandler()
    handler.setLevel(logging.INFO)
    logger.addHandler(handler)
    return handler


# FIXME: We should decide whether we want to make this specific to web-platform-tests or to make it generic to any git repository containing tests.
def parse_args(args):
    description = """
To import a web-platform-tests test suite named xyz, use: 'import-w3c-tests web-platform-tests/xyz'.
To import a web-platform-tests suite from a specific folder, use 'import-w3c-tests xyz -l -s my-folder-containing-web-platform-tests-folder'"""
    parser = argparse.ArgumentParser(prog='import-w3c-tests [web-platform-tests/test-suite-name...]', description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('-n', '--no-overwrite', dest='overwrite', action='store_false', default=True,
        help='Flag to prevent duplicate test files from overwriting existing tests. By default, they will be overwritten')
    parser.add_argument('-l', '--no-links-conversion', dest='convert_test_harness_links', action='store_false', default=True,
       help='Do not change links (testharness js or css e.g.). This option only applies when providing a source directory, in which case by default, links are converted to point to WebKit testharness files. When tests are downloaded from W3C repository, links are converted for CSS tests and remain unchanged for WPT tests')

    parser.add_argument('-t', '--tip-of-tree', dest='use_tip_of_tree', action='store_true', default=False,
        help='Import all tests using the latest repository revision')

    parser.add_argument('-a', '--all', action='store_true', default=False,
        help='Import all tests including reftests, JS tests, and manual/pixel tests. By default, only reftests and JS tests are imported')
    fs = FileSystem()
    parser.add_argument('-d', '--dest-dir', dest='destination', default=fs.join('imported', 'w3c'),
        help='Import into a specified directory relative to the LayoutTests root. By default, imports into imported/w3c')

    parser.add_argument('-s', '--src-dir', dest='source', default=None,
        help='Import from a specific folder which contains web-platform-tests folder. If not provided, the script will clone the necessary repositories.')

    parser.add_argument('-v', '--verbose', action='store_true', default=False,
         help='Print maximal log')
    parser.add_argument('--no-fetch', action='store_false', dest='fetch', default=True,
         help='Do not fetch the repositories. By default, repositories are fetched if a source directory is not provided')
    parser.add_argument('--import-all', action='store_true', default=False,
         help='Ignore the import-expectations.json file. All tests will be imported. This option only applies when tests are downloaded from W3C repository')

    parser.add_argument('--clean-dest-dir', action='store_true', dest='clean_destination_directory', default=False,
         help='Clean destination directory. All files in the destination directory will be deleted except for WebKit specific files (test expectations, .gitignore...) before new tests import. Dangling test expectations (expectation file that is no longer related to a test) are removed after tests import.')

    options, args = parser.parse_known_args(args)
    return options, args


class TestImporter(object):

    def __init__(self, host, test_paths, options):
        self.host = host
        self.source_directory = options.source
        self.options = options
        self.test_paths = test_paths if test_paths else []

        self.filesystem = self.host.filesystem

        webkit_finder = WebKitFinder(self.filesystem)
        self._webkit_root = webkit_finder.webkit_base()

        self.destination_directory = webkit_finder.path_from_webkit_base("LayoutTests", options.destination)
        self.tests_w3c_relative_path = self.filesystem.join('imported', 'w3c')
        self.layout_tests_path = webkit_finder.path_from_webkit_base('LayoutTests')
        self.layout_tests_w3c_path = self.filesystem.join(self.layout_tests_path, self.tests_w3c_relative_path)
        self.tests_download_path = WPTPaths.checkout_directory(webkit_finder)

        self._test_downloader = None

        self._potential_test_resource_files = []

        self.import_list = []
        self._importing_downloaded_tests = self.source_directory is None

        self._test_resource_files_json_path = self.filesystem.join(self.layout_tests_w3c_path, "resources", "resource-files.json")
        self._test_resource_files = json.loads(self.filesystem.read_text_file(self._test_resource_files_json_path)) if self.filesystem.exists(self._test_resource_files_json_path) else None

        self._tests_options_json_path = self.filesystem.join(self.layout_tests_path, 'tests-options.json')
        self._tests_options = json.loads(self.filesystem.read_text_file(self._tests_options_json_path)) if self.filesystem.exists(self._tests_options_json_path) else None
        self._slow_tests = []

        if self.options.clean_destination_directory and self._test_resource_files:
            self._test_resource_files["files"] = []
            if self._tests_options:
                self.remove_slow_from_w3c_tests_options()

    def do_import(self):
        if not self.source_directory:
            _log.info('Downloading W3C test repositories')
            self.source_directory = self.filesystem.join(self.tests_download_path, 'to-be-imported')
            self.filesystem.maybe_make_directory(self.tests_download_path)
            self.filesystem.maybe_make_directory(self.source_directory)
            self.test_downloader().download_tests(self.source_directory, self.test_paths, self.options.use_tip_of_tree)

        test_paths = self.test_paths if self.test_paths else [test_repository['name'] for test_repository in self.test_downloader().test_repositories]
        for test_path in test_paths:
            self.find_importable_tests(self.filesystem.join(self.source_directory, test_path))

        if self.options.clean_destination_directory:
            for test_path in test_paths:
                self.clean_destination_directory(test_path)

        self.import_tests()

        for test_path in test_paths:
            self.remove_dangling_expectations(test_path)

        if self._importing_downloaded_tests:
            self.generate_git_submodules_description_for_all_repositories()

        self.test_downloader().update_import_expectations(self.test_paths)

    def generate_git_submodules_description_for_all_repositories(self):
        for test_repository in self._test_downloader.test_repositories:
            if 'generate_git_submodules_description' in test_repository['import_options']:
                self.filesystem.maybe_make_directory(self.filesystem.join(self.destination_directory, 'resources'))
                self._test_downloader.generate_git_submodules_description(test_repository, self.filesystem.join(self.destination_directory, 'resources', test_repository['name'] + '-modules.json'))
            if 'generate_gitignore' in test_repository['import_options']:
                self._test_downloader.generate_gitignore(test_repository, self.destination_directory)
            if 'generate_init_py' in test_repository['import_options']:
                self.write_init_py(self.filesystem.join(self.destination_directory, test_repository['name'], '__init__.py'))

    def write_init_py(self, filepath):
        self.filesystem.write_text_file(filepath, '# This file is required for Python to search this directory for modules.')

    def test_downloader(self):
        if not self._test_downloader:
            download_options = TestDownloader.default_options()
            download_options.fetch = self.options.fetch
            download_options.verbose = self.options.verbose
            download_options.import_all = self.options.import_all
            self._test_downloader = TestDownloader(self.tests_download_path, self.host, download_options)
        return self._test_downloader

    def should_skip_file(self, filename):
        # For some reason the w3c repo contains random perl scripts we don't care about.
        if filename.endswith('.pl'):
            return True
        if filename.startswith('.'):
            return not filename == '.htaccess'
        return False

    def _is_baseline(self, filesystem, dirname, filename):
        return filename.endswith('-expected.txt')

    def _should_remove_before_importing(self, filesystem, dirname, filename):
        if self._is_baseline(filesystem, dirname, filename):
            return False
        if filename.startswith("."):
            return False
        return True

    def clean_destination_directory(self, filename):
        directory = self.filesystem.join(self.destination_directory, filename)
        for relative_path in self.filesystem.files_under(directory, file_filter=self._should_remove_before_importing):
            self.filesystem.remove(self.filesystem.join(directory, relative_path))

    def remove_dangling_expectations(self, filename):
        #FIXME: Clean also the expected files stored in all platform specific folders.
        directory = self.filesystem.join(self.destination_directory, filename)
        for relative_path in self.filesystem.files_under(directory, file_filter=self._is_baseline):
            path = self.filesystem.join(directory, relative_path)
            if self.filesystem.glob(path.replace('-expected.txt', '*')) == [path]:
                self.filesystem.remove(path)

    def _source_root_directory_for_path(self, path):
        if not self._importing_downloaded_tests:
            return self.source_directory
        for test_repository in self.test_downloader().load_test_repositories(self.filesystem):
            source_directory = self.filesystem.join(self.source_directory, test_repository['name'])
            if path.startswith(source_directory):
                return source_directory

    def find_importable_tests(self, directory):
        def should_keep_subdir(filesystem, path):
            if self._importing_downloaded_tests:
                return True
            subdir = path[len(directory):]
            DIRS_TO_SKIP = ('work-in-progress', 'tools', 'support')
            should_skip = filesystem.basename(subdir).startswith('.') or (subdir in DIRS_TO_SKIP)
            return not should_skip

        source_root_directory = self._source_root_directory_for_path(directory)
        directories = self.filesystem.dirs_under(directory, should_keep_subdir)
        for root in directories:
            _log.info('Scanning ' + root + '...')
            total_tests = 0
            reftests = 0
            jstests = 0

            copy_list = []

            for filename in self.filesystem.listdir(root):
                if self.filesystem.isdir(self.filesystem.join(root, filename)):
                    continue
                # FIXME: This block should really be a separate function, but the early-continues make that difficult.

                if self.should_skip_file(filename):
                    continue

                fullpath = self.filesystem.join(root, filename)

                mimetype = mimetypes.guess_type(fullpath)
                if not 'html' in str(mimetype[0]) and not 'application/xhtml+xml' in str(mimetype[0]) and not 'application/xml' in str(mimetype[0]):
                    copy_list.append({'src': fullpath, 'dest': filename})
                    continue

                test_parser = TestParser(vars(self.options), filename=fullpath, host=self.host, source_root_directory=source_root_directory)
                test_info = test_parser.analyze_test()
                if test_info is None:
                    # This is probably a resource file, but we should generate WPT manifest instead and get the list of resource files from it.
                    if not self._is_in_resources_directory(fullpath):
                        self._potential_test_resource_files.append(fullpath)
                    copy_list.append({'src': fullpath, 'dest': filename})
                    continue
                elif self._is_in_resources_directory(fullpath):
                    _log.warning('%s is a test located in a "resources" folder. This test will be skipped by WebKit test runners.', fullpath)

                if 'manualtest' in test_info.keys():
                    continue

                if 'slow' in test_info:
                    self._slow_tests.append(fullpath)

                if 'referencefile' in test_info.keys():
                    # Skip it since, the corresponding reference test should have a link to this file
                    continue

                if 'reference' in test_info.keys():
                    reftests += 1
                    total_tests += 1
                    test_basename = self.filesystem.basename(test_info['test'])

                    # Add the ref file, following WebKit style.
                    # FIXME: Ideally we'd support reading the metadata
                    # directly rather than relying  on a naming convention.
                    # Using a naming convention creates duplicate copies of the
                    # reference files.
                    ref_file = self.filesystem.splitext(test_basename)[0] + '-expected'
                    ref_file += self.filesystem.splitext(test_info['reference'])[1]

                    copy_list.append({'src': test_info['reference'], 'dest': ref_file, 'reference_support_info': test_info['reference_support_info']})
                    copy_list.append({'src': test_info['test'], 'dest': filename})

                elif 'jstest' in test_info.keys():
                    jstests += 1
                    total_tests += 1
                    copy_list.append({'src': fullpath, 'dest': filename})
                else:
                    total_tests += 1
                    copy_list.append({'src': fullpath, 'dest': filename})

            if copy_list:
                # Only add this directory to the list if there's something to import
                self.import_list.append({'dirname': root, 'copy_list': copy_list,
                    'reftests': reftests, 'jstests': jstests, 'total_tests': total_tests})

    def should_convert_test_harness_links(self, test):
        if self._importing_downloaded_tests:
            for test_repository in self.test_downloader().test_repositories:
                if test.startswith(test_repository['name']):
                    return 'convert_test_harness_links' in test_repository['import_options']
            return True
        return self.options.convert_test_harness_links

    def _webkit_test_runner_options(self, path):
        if not(self.filesystem.isfile(path)):
            return ''

        options_prefix = '<!-- webkit-test-runner'
        contents = ''
        try:
            contents = self.filesystem.read_text_file(path).split('\n')
        except:
            _log.info('unable to read %s as a text file' % path)

        if not len(contents):
            return ''
        first_line = contents[0]

        return first_line[first_line.index(options_prefix):] if options_prefix in first_line else ''

    def _add_webkit_test_runner_options_to_content(self, content, webkit_test_runner_options):
        lines = content.split('\n')
        if not len(lines):
            return ''
        lines[0] = lines[0] + webkit_test_runner_options
        return '\n'.join(lines)

    def _copy_html_file(self, source_filepath, new_filepath):
        webkit_test_runner_options = self._webkit_test_runner_options(new_filepath)
        if not webkit_test_runner_options:
            self.filesystem.copyfile(source_filepath, new_filepath)
            return

        source_content = self.filesystem.read_text_file(source_filepath)
        self.filesystem.write_text_file(new_filepath, self._add_webkit_test_runner_options_to_content(source_content, webkit_test_runner_options))

    def _write_html_template(self, new_filepath):
        webkit_test_runner_options = self._webkit_test_runner_options(new_filepath)
        content = '<!-- This file is required for WebKit test infrastructure to run the templated test -->'
        self.filesystem.write_text_file(new_filepath, content + webkit_test_runner_options)

    def write_html_files_for_templated_js_tests(self, orig_filepath, new_filepath):
        if (orig_filepath.endswith('.window.js')):
            self._write_html_template(new_filepath.replace('.window.js', '.window.html'))
            return
        if (orig_filepath.endswith('.worker.js')):
            self._write_html_template(new_filepath.replace('.worker.js', '.worker.html'))
            return
        if (orig_filepath.endswith('.any.js')):
            self._write_html_template(new_filepath.replace('.any.js', '.any.html'))
            self._write_html_template(new_filepath.replace('.any.js', '.any.worker.html'))
            return

    def import_tests(self):
        total_imported_tests = 0
        total_imported_reftests = 0
        total_imported_jstests = 0
        total_prefixed_properties = {}
        total_prefixed_property_values = {}

        failed_conversion_files = []

        # We currently need to rewrite css web-platform-tests because they use a separate "reference" folder for
        # their ref-tests' results.
        folders_needing_file_rewriting = ['web-platform-tests/css', ]

        for dir_to_copy in self.import_list:
            total_imported_tests += dir_to_copy['total_tests']
            total_imported_reftests += dir_to_copy['reftests']
            total_imported_jstests += dir_to_copy['jstests']

            prefixed_properties = []
            prefixed_property_values = []

            if not dir_to_copy['copy_list']:
                continue

            orig_path = dir_to_copy['dirname']

            subpath = self.filesystem.relpath(orig_path, self.source_directory)
            new_path = self.filesystem.join(self.destination_directory, subpath)

            should_rewrite_files = False
            for folder_needing_file_rewriting in folders_needing_file_rewriting:
                if subpath.startswith(folder_needing_file_rewriting):
                    should_rewrite_files = True
                    break

            if not(self.filesystem.exists(new_path)):
                self.filesystem.maybe_make_directory(new_path)

            copied_files = []

            for file_to_copy in dir_to_copy['copy_list']:
                # FIXME: Split this block into a separate function.
                orig_filepath = self.filesystem.normpath(file_to_copy['src'])

                if self.filesystem.isdir(orig_filepath):
                    # FIXME: Figure out what is triggering this and what to do about it.
                    _log.error('%s refers to a directory' % orig_filepath)
                    continue

                if not(self.filesystem.exists(orig_filepath)):
                    _log.warning('%s not found. Possible error in the test.', orig_filepath)
                    continue

                new_filepath = self.filesystem.join(new_path, file_to_copy['dest'])
                if 'reference_support_info' in file_to_copy.keys() and file_to_copy['reference_support_info'] != {}:
                    reference_support_info = file_to_copy['reference_support_info']
                else:
                    reference_support_info = None

                if not(self.filesystem.exists(self.filesystem.dirname(new_filepath))):
                    self.filesystem.maybe_make_directory(self.filesystem.dirname(new_filepath))

                if not self.options.overwrite and self.filesystem.exists(new_filepath):
                    _log.info('Skipping import of existing file ' + new_filepath)
                else:
                    # FIXME: Maybe doing a file diff is in order here for existing files?
                    # In other words, there's no sense in overwriting identical files, but
                    # there's no harm in copying the identical thing.
                    _log.info('Importing: %s', orig_filepath)
                    _log.info('       As: %s', new_filepath)

                # Only html, xml, or css should be converted
                # FIXME: Eventually, so should js when support is added for this type of conversion
                mimetype = mimetypes.guess_type(orig_filepath)
                if should_rewrite_files and ('html' in str(mimetype[0]) or 'xml' in str(mimetype[0])  or 'css' in str(mimetype[0])):
                    try:
                        converted_file = convert_for_webkit(new_path, filename=orig_filepath, reference_support_info=reference_support_info, host=self.host, convert_test_harness_links=self.should_convert_test_harness_links(subpath), webkit_test_runner_options=self._webkit_test_runner_options(new_filepath))
                    except:
                        _log.warn('Failed converting %s', orig_filepath)
                        failed_conversion_files.append(orig_filepath)
                        converted_file = None

                    if not converted_file:
                        self.filesystem.copyfile(orig_filepath, new_filepath)  # The file was unmodified.
                    else:
                        for prefixed_property in converted_file[0]:
                            total_prefixed_properties.setdefault(prefixed_property, 0)
                            total_prefixed_properties[prefixed_property] += 1

                        prefixed_properties.extend(set(converted_file[0]) - set(prefixed_properties))

                        for prefixed_value in converted_file[1]:
                            total_prefixed_property_values.setdefault(prefixed_value, 0)
                            total_prefixed_property_values[prefixed_value] += 1

                        prefixed_property_values.extend(set(converted_file[1]) - set(prefixed_property_values))

                        self.filesystem.write_binary_file(new_filepath, converted_file[2])
                elif orig_filepath.endswith('__init__.py') and not self.filesystem.getsize(orig_filepath):
                    # Some bots dislike empty __init__.py.
                    self.write_init_py(new_filepath)
                elif 'html' in str(mimetype[0]):
                    self._copy_html_file(orig_filepath, new_filepath)
                else:
                    self.filesystem.copyfile(orig_filepath, new_filepath)

                self.write_html_files_for_templated_js_tests(orig_filepath, new_filepath)

                copied_files.append(new_filepath.replace(self._webkit_root, ''))

            self.remove_deleted_files(new_path, copied_files)
            self.write_import_log(new_path, copied_files, prefixed_properties, prefixed_property_values)

        _log.info('Import complete')

        _log.info('IMPORTED %d TOTAL TESTS', total_imported_tests)
        _log.info('Imported %d reftests', total_imported_reftests)
        _log.info('Imported %d JS tests', total_imported_jstests)
        _log.info('Imported %d pixel/manual tests', total_imported_tests - total_imported_jstests - total_imported_reftests)
        if len(failed_conversion_files):
            _log.warn('Failed converting %d files (files copied without being converted)', len(failed_conversion_files))
        _log.info('')
        _log.info('Properties needing prefixes (by count):')

        for prefixed_property in sorted(total_prefixed_properties, key=lambda p: total_prefixed_properties[p]):
            _log.info('  %s: %s', prefixed_property, total_prefixed_properties[prefixed_property])
        _log.info('')
        _log.info('Property values needing prefixes (by count):')

        for prefixed_value in sorted(total_prefixed_property_values, key=lambda p: total_prefixed_property_values[p]):
            _log.info('  %s: %s', prefixed_value, total_prefixed_property_values[prefixed_value])

        if self._potential_test_resource_files and self._test_resource_files:
            # FIXME: We should check that actual tests are not in the test_resource_files list
            should_update_json_file = False
            files = self._test_resource_files["files"]
            for full_path in self._potential_test_resource_files:
                resource_file_path = self.filesystem.relpath(full_path, self.source_directory)
                if not self._already_identified_as_resource_file(resource_file_path):
                    files.append(resource_file_path)
                    should_update_json_file = True
            if should_update_json_file:
                files.sort()
                self.filesystem.write_text_file(self._test_resource_files_json_path, json.dumps(self._test_resource_files, sort_keys=True, indent=4).replace(' \n', '\n'))

        if self._tests_options:
            self.update_tests_options()

    def _already_identified_as_resource_file(self, path):
        if not self._test_resource_files:
            return False
        if path in self._test_resource_files["files"]:
            return True
        return any([path.find(directory) != -1 for directory in self._test_resource_files["directories"]])

    def _is_in_resources_directory(self, path):
        return "resources" in path.split(self.filesystem.sep)

    def update_tests_options(self):
        should_update = self.options.clean_destination_directory
        for full_path in self._slow_tests:
            w3c_test_path = self.filesystem.relpath(full_path, self.source_directory)
            print(w3c_test_path)
            # No need to mark tests as slow if they are in skipped directories
            if self._already_identified_as_resource_file(w3c_test_path):
                continue

            test_path = self.filesystem.join(self.tests_w3c_relative_path, w3c_test_path)
            options = self._tests_options.get(test_path, [])
            if not 'slow' in options:
                options.append('slow')
                self._tests_options[test_path] = options
                should_update = True

        if should_update:
            self.filesystem.write_text_file(self._tests_options_json_path, json.dumps(self._tests_options, sort_keys=True, indent=4).replace(' \n', '\n'))

    def remove_slow_from_w3c_tests_options(self):
        for test_path in self._tests_options.keys():
            if self.tests_w3c_relative_path in test_path:
                options = self._tests_options[test_path]
                options.remove('slow')
                if not options:
                    self._tests_options.pop(test_path)

    def remove_deleted_files(self, import_directory, new_file_list):
        """ Reads an import log in |import_directory|, compares it to the |new_file_list|, and removes files not in the new list."""

        previous_file_list = []

        import_log_file = self.filesystem.join(import_directory, 'w3c-import.log')
        if not self.filesystem.exists(import_log_file):
            return

        contents = self.filesystem.read_text_file(import_log_file).split('\n')

        if 'List of files:' in contents:
            list_index = contents.index('List of files:') + 1
            previous_file_list = [filename.strip() for filename in contents[list_index:] if len(filename)]

        deleted_files = set(previous_file_list) - set(new_file_list)
        for deleted_file in deleted_files:
            _log.info('Deleting file removed from the W3C repo: %s', deleted_file)
            deleted_file = self.filesystem.join(self._webkit_root, deleted_file[1:])
            if not self.filesystem.exists(deleted_file):
                _log.warning('%s no longer exists', deleted_file)
                continue
            self.filesystem.remove(deleted_file)

    def write_import_log(self, import_directory, file_list, prop_list, property_values_list):
        """ Writes a w3c-import.log file in each directory with imported files. """

        import_log = []
        import_log.append('The tests in this directory were imported from the W3C repository.\n')
        import_log.append('Do NOT modify these tests directly in WebKit.\n')
        import_log.append('Instead, create a pull request on the WPT github:\n')
        import_log.append('\t%s\n\n' % WPT_GH_URL)
        import_log.append('Then run the Tools/Scripts/import-w3c-tests in WebKit to reimport\n\n')
        import_log.append('Do NOT modify or remove this file.\n\n')
        import_log.append('------------------------------------------------------------------------\n')
        import_log.append('Properties requiring vendor prefixes:\n')
        if prop_list:
            for prop in prop_list:
                import_log.append(prop + '\n')
        else:
            import_log.append('None\n')
        import_log.append('Property values requiring vendor prefixes:\n')
        if property_values_list:
            for value in property_values_list:
                import_log.append(value + '\n')
        else:
            import_log.append('None\n')
        import_log.append('------------------------------------------------------------------------\n')
        import_log.append('List of files:\n')
        for item in sorted(file_list):
            import_log.append(item + '\n')

        self.filesystem.write_text_file(self.filesystem.join(import_directory, 'w3c-import.log'), ''.join(import_log))
