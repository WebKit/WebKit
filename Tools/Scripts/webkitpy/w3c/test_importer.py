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

    - By default, only manual, reftests, jstest and crash tests are imported. This can be overridden with a -a or --all
      argument

    - Also by default, if test files by the same name already exist in the destination directory,
      they are overwritten with the idea that running this script would refresh files periodically.
      This can also be overridden by a -n or --no-overwrite flag

    - If no import_directory is provided, the script will download the tests from the W3C github repositories.
      The selection of tests and folders to import will be based on
      LayoutTests/imported/w3c/resources/import-expectations.json which lists the test suites or tests to
      import or not.

    - All files are converted to work in WebKit:
         1. All CSS properties requiring the -webkit-vendor prefix are prefixed - this current
            list of what needs prefixes is read from Source/WebCore/CSS/CSSProperties.in
         2. Each reftest has its own copy of its reference file following the naming conventions
            new-run-webkit-tests expects
         3. If a reference files lives outside the directory of the test that uses it, it is checked
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
import itertools
import json
import logging
import mimetypes

try:
    from pathlib import Path
except ImportError:
    from pathlib2 import Path

from webkitpy.common.host import Host
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.port.factory import PortFactory
from webkitpy.layout_tests.controllers.layout_test_finder_legacy import LayoutTestFinder
from webkitpy.w3c.common import TEMPLATED_TEST_HEADER, WPT_GH_URL, WPTPaths
from webkitpy.w3c.test_parser import TestParser
from webkitpy.w3c.test_converter import convert_for_webkit
from webkitpy.w3c.test_downloader import TestDownloader

_log = logging.getLogger(__name__)


def main(_argv, _stdout, _stderr):
    options, test_paths = parse_args(_argv)

    configure_logging()

    host = Host()
    port = host.port_factory.get()

    test_importer = TestImporter(port, test_paths, options)
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
To import a web-platform-tests test suite named xyz, use:
    import-w3c-tests --tip-of-tree web-platform-tests/xyz

To import a web-platform-tests suite from a local copy of web platform tests:
   1. Your local WPT copy must be in a directory called "web-platform-tests".
   2. If the local copy is at, for example, "~/dev/web-platform-tests/", use:
      import-w3c-tests web-platform-tests/xyz --src-dir ~/dev/"""
    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('-n', '--no-overwrite', dest='overwrite', action='store_false', default=True,
                        help='Flag to prevent duplicate test files from overwriting existing tests. By default, they will be overwritten')

    parser.add_argument('-t', '--tip-of-tree', dest='use_tip_of_tree', action='store_true', default=False,
                        help='Import all tests using the latest repository revision')

    parser.add_argument('-a', '--all', action='store_true', default=False,
                        help='Import all tests. By default, only manual, reftests, JS and crash tests are imported.')

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

    parser.add_argument('--no-clean-dest-dir', action='store_false', dest='clean_destination_directory', default=True,
                        help='Do not clean the destination directory. By default all files in the destination directory will be deleted except for WebKit specific files (test expectations, .gitignore...) before new tests import. Dangling test expectations (expectation file that is no longer related to a test) are removed after tests import.')

    parser.add_argument('test_paths', metavar='web-platform-tests/test_path', nargs='*',
                        help='directories to import')

    args = parser.parse_args(args)
    return args, args.test_paths


class TestImporter(object):

    def __init__(self, port, test_paths, options):
        self.host = port.host
        self.port = port
        self.source_directory = options.source
        self.options = options
        self.test_paths = test_paths if test_paths else []

        self.filesystem = self.host.filesystem

        webkit_finder = WebKitFinder(self.filesystem)
        self._webkit_root = webkit_finder.webkit_base()

        self.destination_directory = self.port.path_from_webkit_base("LayoutTests", options.destination)
        self.tests_w3c_relative_path = self.filesystem.join('imported', 'w3c')
        self.layout_tests_path = self.port.path_from_webkit_base('LayoutTests')
        self.layout_tests_w3c_path = self.filesystem.join(self.layout_tests_path, self.tests_w3c_relative_path)
        self.tests_download_path = WPTPaths.checkout_directory(webkit_finder)

        self._test_downloader = None

        self._potential_test_resource_files = []

        self.import_list = []
        self.upstream_revision = None

        self._test_resource_files_json_path = self.filesystem.join(self.layout_tests_w3c_path, "resources", "resource-files.json")
        self._test_resource_files = json.loads(self.filesystem.read_text_file(self._test_resource_files_json_path)) if self.filesystem.exists(self._test_resource_files_json_path) else None

        self._tests_options_json_path = self.filesystem.join(self.layout_tests_path, 'tests-options.json')
        self._tests_options = json.loads(self.filesystem.read_text_file(self._tests_options_json_path)) if self.filesystem.exists(self._tests_options_json_path) else None
        self._slow_tests = []

        self._to_skip_new_directories = set()

        self.globalToSuffixes = {
            'window': ('html',),
            'worker': ('worker.html', 'serviceworker.html', 'sharedworker.html'),
            'dedicatedworker': ('worker.html',),
            'serviceworker': ('serviceworker.html',),
            'serviceworker-module': ('serviceworker-module.html',),
            'sharedworker': ('sharedworker.html',)
        }

    def do_import(self):
        if self.source_directory:
            source_path = str(Path(self.source_directory) / 'web-platform-tests')
            try:
                git = self.test_downloader().git(source_path)
                self.upstream_revision = git.rev_parse('HEAD')
            except (OSError, ScriptError):
                pass
        else:
            _log.info('Downloading W3C test repositories')
            self.filesystem.maybe_make_directory(self.tests_download_path)
            self.test_downloader().download_tests(self.options.use_tip_of_tree)
            self.upstream_revision = self.test_downloader().upstream_revision
            self.source_directory = self.tests_download_path

        for test_path in self.test_paths:
            if test_path != "web-platform-tests" and not test_path.startswith(
                "web-platform-tests" + self.filesystem.sep
            ):
                _log.error(
                    "All test paths must start with 'web-platform-tests%s'; %r does not"
                    % (self.filesystem.sep, test_path)
                )
                return

        test_paths = self.test_paths if self.test_paths else [test_repository['name'] for test_repository in self.test_downloader().test_repositories]

        test_paths = (
            [p.rstrip(self.filesystem.sep) + self.filesystem.sep for p in test_paths]
            if test_paths
            else []
        )

        for test_path in test_paths:
            self.find_importable_tests(self.filesystem.join(self.source_directory, test_path))

        if self.options.clean_destination_directory:
            for test_path in test_paths:
                self.clean_destination_directory(test_path)
            if self._test_resource_files:
                test_paths_tuple = tuple(test_paths)
                self._test_resource_files["files"] = [t for t in self._test_resource_files["files"]
                                                      if not t.startswith(test_paths_tuple)]
                if self._tests_options:
                    self.remove_slow_from_w3c_tests_options(test_paths_tuple)

        self.import_tests()

        for test_path in test_paths:
            self.remove_dangling_expectations(test_path)

        self.generate_git_submodules_description_for_all_repositories()

        self.test_downloader().update_import_expectations(
            self.test_paths, self._to_skip_new_directories
        )

    def generate_git_submodules_description_for_all_repositories(self):
        for test_repository in self._test_downloader.test_repositories:
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
            self._test_downloader = TestDownloader(self.tests_download_path, self.port, download_options)
        return self._test_downloader

    def should_skip_path(self, path):
        rel_path = Path(path).relative_to(self.source_directory)
        if rel_path.suffix == ".pl":
            return True

        if rel_path.name.startswith(".") and rel_path.name != ".htaccess":
            return True

        downloader = self.test_downloader()
        paths_to_skip_new_directories = {Path(p) for p in downloader.paths_to_skip_new_directories}
        paths_to_skip = {Path(p) for p in downloader.paths_to_skip}
        paths_to_import = {Path(p) for p in downloader.paths_to_import}

        for parent in itertools.chain([rel_path], rel_path.parents):
            if parent in paths_to_skip_new_directories:
                if parent != rel_path:
                    to_skip = parent / rel_path.relative_to(parent).parts[0]
                    full_to_skip = str(self.source_directory / to_skip)
                    assert self.filesystem.exists(full_to_skip)
                    if not self.filesystem.isdir(full_to_skip):
                        # Files directly under skip-new-directories _are_ imported.
                        assert rel_path == to_skip
                        return False
                    self._to_skip_new_directories.add(str(to_skip))
                return True

            if parent in paths_to_skip:
                return True

            if parent in paths_to_import:
                return False

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
        tests = LayoutTestFinder(self.port, None).find_tests_by_path([directory])
        baselines_for_tests = {
            self.filesystem.join(
                platform_dir or self.port.layout_tests_dir(), baseline_filename
            )
            for test in tests
            for platform_dir, baseline_filename in self.port.expected_baselines(
                test.test_path, ".txt", all_baselines=True
            )
        }
        for relative_path in self.filesystem.files_under(directory, file_filter=self._is_baseline):
            path = self.filesystem.join(directory, relative_path)
            if path not in baselines_for_tests:
                self.filesystem.remove(path)

    def _source_root_directory_for_path(self, path):
        for test_repository in self.test_downloader().load_test_repositories(self.filesystem):
            source_directory = self.filesystem.join(self.source_directory, test_repository['name'])
            if path.startswith(source_directory):
                return source_directory

    def find_importable_tests(self, directory):
        source_root_directory = self._source_root_directory_for_path(directory)
        directories = self.filesystem.dirs_under(directory)
        for root in directories:
            _log.info('Scanning ' + root + '...')
            total_tests = 0
            reftests = 0
            jstests = 0
            crashtests = 0

            copy_list = []

            for filename in self.filesystem.listdir(root):
                if self.filesystem.isdir(self.filesystem.join(root, filename)):
                    continue
                # FIXME: This block should really be a separate function, but the early-continues make that difficult.

                fullpath = self.filesystem.join(root, filename)

                if self.should_skip_path(fullpath):
                    continue

                mimetype = mimetypes.guess_type(fullpath)
                if 'html' not in str(mimetype[0]) and 'application/xhtml+xml' not in str(mimetype[0]) and 'application/xml' not in str(mimetype[0]) and 'image/svg+xml' not in str(mimetype[0]):
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

                if 'match_reference' in test_info.keys() or 'mismatch_reference' in test_info.keys():
                    reftests += 1
                    total_tests += 1
                    test_basename = self.filesystem.basename(test_info['test'])

                    ref_files = []

                    # Add the ref file, following WebKit style.
                    # FIXME: Ideally we'd support reading the metadata
                    # directly rather than relying  on a naming convention.
                    # Using a naming convention creates duplicate copies of the
                    # reference files.
                    if 'match_reference' in test_info.keys():
                        ref_file = self.filesystem.splitext(test_basename)[0] + '-expected'
                        ref_file += self.filesystem.splitext(test_info['match_reference'])[1]
                        copy_list.append({'src': test_info['match_reference'], 'dest': ref_file, 'reference_support_info': test_info['match_reference_support_info']})
                        ref_files.append({'src': self.filesystem.split(test_info['match_reference'])[1], 'dest': self.filesystem.split(ref_file)[1]})

                    if 'mismatch_reference' in test_info.keys():
                        ref_file = self.filesystem.splitext(test_basename)[0] + '-expected-mismatch'
                        ref_file += self.filesystem.splitext(test_info['mismatch_reference'])[1]
                        copy_list.append({'src': test_info['mismatch_reference'], 'dest': ref_file, 'reference_support_info': test_info['mismatch_reference_support_info']})
                        ref_files.append({'src': self.filesystem.split(test_info['mismatch_reference'])[1], 'dest': self.filesystem.split(ref_file)[1]})

                    copy_list.append({'src': test_info['test'], 'dest': filename, 'reference_file_renames': ref_files})

                elif 'jstest' in test_info.keys():
                    jstests += 1
                    total_tests += 1
                    copy_list.append({'src': fullpath, 'dest': filename})
                elif 'crashtest' in test_info.keys():
                    crashtests += 1
                    total_tests += 1
                    copy_list.append({'src': fullpath, 'dest': filename})
                else:
                    total_tests += 1
                    copy_list.append({'src': fullpath, 'dest': filename})

            if copy_list:
                # Only add this directory to the list if there's something to import
                self.import_list.append({'dirname': root, 'copy_list': copy_list,
                    'reftests': reftests, 'jstests': jstests, 'crashtests': crashtests, 'total_tests': total_tests})

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

    def _variant_lines(self, variants):
        if not variants:
            return ''
        result = '\n'
        result += '\n'.join(['<!-- META: variant=' + variant + ' -->' for variant in variants])
        return result

    def _write_html_template(self, new_filepath, variants=None):
        webkit_test_runner_options = self._webkit_test_runner_options(new_filepath)

        self.filesystem.write_text_file(new_filepath, TEMPLATED_TEST_HEADER + webkit_test_runner_options + self._variant_lines(variants))

    def _read_environments_and_variants_for_template_test(self, filepath):
        environments = []
        variants = []
        lines = self.filesystem.read_text_file(filepath).split('\n')
        for line in lines:
            if line.startswith('//') and 'META: variant=' in line:
                variant = line.split('META: variant=', 1)[1].strip()
                variants.append(variant)
            if line.startswith('//') and 'META: global=' in line:
                items = line.split('META: global=', 1)[1].split(',')
                suffixes = set()
                for item in items:
                    suffixes_for_item = self.globalToSuffixes.get(item.strip(), ())
                    if len(suffixes_for_item) == 0:
                        suffixes.add('')
                    else:
                        suffixes.update(suffixes_for_item)
                environments = list(filter(None, suffixes))
        return set(environments) if len(environments) else ['html', 'worker.html'], variants

    def write_html_files_for_templated_js_tests(self, orig_filepath, new_filepath):
        if (orig_filepath.endswith('.window.js')):
            self._write_html_template(new_filepath.replace('.window.js', '.window.html'))
            return
        if (orig_filepath.endswith('.worker.js')):
            self._write_html_template(new_filepath.replace('.worker.js', '.worker.html'))
            return
        if (orig_filepath.endswith('.any.js')):
            environments, variants = self._read_environments_and_variants_for_template_test(orig_filepath)
            for suffix in environments:
                self._write_html_template(new_filepath.replace('.any.js', '.any.' + suffix), variants)
            return

    def import_tests(self):
        total_imported_tests = 0
        total_imported_reftests = 0
        total_imported_jstests = 0
        total_imported_crashtests = 0
        total_prefixed_properties = {}
        total_prefixed_property_values = {}

        failed_conversion_files = []

        # We currently need to rewrite css web-platform-tests because they use a separate "reference" folder for
        # their ref-tests' results.
        folders_needing_file_rewriting = ['web-platform-tests/']

        for dir_to_copy in self.import_list:
            total_imported_tests += dir_to_copy['total_tests']
            total_imported_reftests += dir_to_copy['reftests']
            total_imported_jstests += dir_to_copy['jstests']
            total_imported_crashtests += dir_to_copy['crashtests']

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

                if 'reference_file_renames' in file_to_copy.keys() and file_to_copy['reference_file_renames'] != {}:
                    reference_file_renames = file_to_copy['reference_file_renames']
                else:
                    reference_file_renames = []

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
                if should_rewrite_files and ('text/' in str(mimetype[0]) or 'application/' in str(mimetype[0])) \
                                        and ('html' in str(mimetype[0]) or 'xml' in str(mimetype[0])  or 'css' in str(mimetype[0]) or 'javascript' in str(mimetype[0])):
                    _log.info("Rewriting: %s" % new_filepath)
                    try:
                        converted_file = convert_for_webkit(new_path, filename=orig_filepath, reference_support_info=reference_support_info, reference_file_renames=reference_file_renames, host=self.host, webkit_test_runner_options=self._webkit_test_runner_options(new_filepath))
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
        _log.info('Imported %d Crash tests', total_imported_crashtests)
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

        if self.upstream_revision:
            _log.info('\n--------- Please include the following in your commit message: ---------\n')
            _log.info('Upstream commit: https://github.com/web-platform-tests/wpt/commit/%s', self.upstream_revision)
            _log.info('-' * 72)

        if self._test_resource_files:
            # FIXME: We should check that actual tests are not in the test_resource_files list
            should_update_json_file = self.options.clean_destination_directory
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

    def remove_slow_from_w3c_tests_options(self, test_paths):
        full_test_paths = tuple(self.filesystem.join(self.tests_w3c_relative_path, test_path) for test_path in test_paths)

        to_remove = set()
        for test_path in self._tests_options.keys():
            if self.tests_w3c_relative_path in test_path and test_path.startswith(full_test_paths):
                options = self._tests_options[test_path]
                options.remove('slow')
                if not options:
                    to_remove.add(test_path)

        for old in to_remove:
            del self._tests_options[old]

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
