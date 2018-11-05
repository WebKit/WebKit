# Copyright (C) 2018 Igalia S.L.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import json
import logging
import optparse
import os
import sys

from webkitpy.common.host import Host
from webkitpy.common.system.logutils import configure_logging
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.w3c.common import WPTPaths
from webkitpy.w3c.test_downloader import TestDownloader
from webkitpy.webdriver_tests.webdriver_driver import create_driver

_log = logging.getLogger(__name__)


def main(script_name, argv):
    options, args = parse_args(argv)

    configure_logging(logging.DEBUG if options.verbose else logging.INFO)

    # Determine Port for the specified platform.
    host = Host()
    try:
        port = host.port_factory.get(options.platform, options)
    except NotImplementedError, e:
        _log.error(str(e))
        sys.exit(-1)

    runner = WPTRunner(port, port.host, script_name, options)
    # If necessary, inject the jhbuild wrapper or re-run in flatpak sandbox.
    if port.name() in ['gtk', 'wpe']:
        filesystem = host.filesystem

        top_level_directory = filesystem.normpath(filesystem.join(filesystem.dirname(__file__), '..', '..', '..', '..'))
        sys.path.insert(0, filesystem.join(top_level_directory, 'Tools', 'flatpak'))
        import flatpakutils
        if not flatpakutils.is_sandboxed() and flatpakutils.check_flatpak(verbose=False):
            flatpak_runner = flatpakutils.WebkitFlatpak.load_from_args(sys.argv)
            if flatpak_runner.clean_args() and flatpak_runner.has_environment():
                if not runner.prepare_wpt_checkout():
                    sys.exit(1)

                hostfilename = os.path.join(flatpak_runner.build_path, "wpt_etc_hosts")
                with open(hostfilename, "w") as stdout:
                    flatpak_runner.run_in_sandbox(os.path.join(options.wpt_checkout, "wpt"), "make-hosts-file", stdout=stdout)

                sys.exit(flatpak_runner.run_in_sandbox(*sys.argv,
                    extra_flatpak_args=['--bind-mount=/etc/hosts=%s' % hostfilename]))

        sys.path.insert(0, filesystem.join(top_level_directory, 'Tools', 'jhbuild'))
        import jhbuildutils

        if not flatpakutils.is_sandboxed() and not jhbuildutils.enter_jhbuild_environment_if_available(port.name()):
            _log.warning('jhbuild environment not present. Run update-webkitgtk-libs before build-webkit to ensure proper testing.')

    # Create the Port-specific driver.
    port._display_server = options.display_server
    display_driver = port.create_driver(worker_number=0, no_timeout=True)._make_driver(pixel_tests=False)
    if not display_driver.check_driver(port):
        raise RuntimeError("Failed to check driver %s" % display_driver.__class__.__name__)
    os.environ.update(display_driver._setup_environ_for_test())

    if not options.child_processes:
        options.child_processes = os.environ.get("WEBKIT_TEST_CHILD_PROCESSES",
                                                 str(port.default_child_processes()))

    if not runner.run(args):
        sys.exit(1)


def parse_args(args):
    option_parser = optparse.OptionParser(usage='usage: %prog [options] [test...]')
    option_parser.add_option('--verbose', action='store_true',
                             help='Show debug message')
    option_parser.add_option('--platform', action='store',
                             help='Platform to use (e.g., "gtk")')
    option_parser.add_option('--gtk', action='store_const', dest='platform', const='gtk',
                             help='Alias for --platform=gtk')
    option_parser.add_option('--wpe', action='store_const', dest='platform', const='wpe',
                             help='Alias for --platform=wpe')
    option_parser.add_option('--child-processes',
                             help='Number of tests to run in parallel'),
    option_parser.add_option('--wpt-checkout', default=None,
                             help='web-platform-tests repository location')
    option_parser.add_option('--wpt-metadata', default=None,
                             help='web-platform-tests metadata location')
    option_parser.add_option('--display-server', choices=['headless', 'xvfb', 'xorg', 'weston', 'wayland'], default='xvfb',
                             help='"headless": Use headless mode. "xvfb": Use a virtualized X11 server. '
                                  '"xorg": Use the current X11 session. "weston": Use a virtualized Weston server. '
                                  '"wayland": Use the current wayland session.')

    return option_parser.parse_args(args)


def create_webdriver(port):
    return create_driver(port)


def spawn_wpt(script_name, wpt_checkout, wpt_args):
    # Import the WPT python module.
    sys.path.insert(0, wpt_checkout)
    try:
        from tools.wpt import wpt
    except ImportError:
        _log.error("Failed to import wpt Python modules from the web-platform-tests directory")
        sys.exit(-1)

    wpt.main(script_name, wpt_args)


class WPTRunner(object):
    def __init__(self, port, host, script_name, options, downloader_class=TestDownloader,
        create_webdriver_func=create_webdriver, spawn_wpt_func=spawn_wpt):
        self._port = port
        self._host = host
        self._finder = WebKitFinder(self._host.filesystem)

        self._script_name = script_name
        self._options = options

        self._downloader_class = downloader_class
        self._create_webdriver_func = create_webdriver_func
        self._spawn_wpt_func = spawn_wpt_func

    def prepare_wpt_checkout(self):
        if not self._options.wpt_checkout:
            test_downloader = self._downloader_class(WPTPaths.checkout_directory(self._finder),
                self._host, self._downloader_class.default_options())
            test_downloader.clone_tests()
            self._options.wpt_checkout = WPTPaths.wpt_checkout_path(self._finder)

        if not self._options.wpt_checkout or not self._host.filesystem.exists(self._options.wpt_checkout):
            _log.error("Valid web-platform-tests directory required")
            return False
        return True

    def _generate_metadata_directory(self, metadata_path):
        expectations_file = self._finder.path_from_webkit_base("WebPlatformTests", self._port.name(), "TestExpectations.json")
        if not self._host.filesystem.exists(expectations_file):
            _log.error("Port-specific expectation .json file does not exist")
            return False

        with self._host.filesystem.open_text_file_for_reading(expectations_file) as fd:
            expectations = json.load(fd)

        self._host.filesystem.rmtree(metadata_path)

        for test_name, test_data in expectations.iteritems():
            ini_file = self._host.filesystem.join(metadata_path, test_name + ".ini")
            self._host.filesystem.maybe_make_directory(self._host.filesystem.dirname(ini_file))
            with self._host.filesystem.open_text_file_for_writing(ini_file) as fd:
                fd.write("[{}]\n".format(test_data.get("test_name", self._host.filesystem.basename(test_name))))
                if "disabled" in test_data:
                    fd.write("    disabled: {}\n".format(test_data["disabled"]))
                elif "expected" in test_data:
                    fd.write("    expected: {}\n".format(test_data["expected"]))
                elif "subtests" in test_data:
                    for subtest_name, subtest_data in test_data["subtests"].iteritems():
                        fd.write("    [{}]\n".format(subtest_name))
                        if "expected" in subtest_data:
                            fd.write("        expected: {}\n".format(subtest_data["expected"]))
                        else:
                            _log.error("Invalid format of TestExpectations.json")
                            return False
                else:
                    _log.error("Invalid format of TestExpectations.json")
                    return False

        return True

    def _wpt_run_paths(self):
        test_manifest_file = (self._port.name(), "TestManifest.ini")
        if not self._options.wpt_metadata:
            return {'metadata_path': self._port.wpt_metadata_directory(),
                'include_manifest_path': self._finder.path_from_webkit_base("WebPlatformTests", *test_manifest_file),
                'manifest_path': self._port.wpt_manifest_file()}

        metadata_path = self._host.filesystem.abspath(self._options.wpt_metadata)
        return {'metadata_path': metadata_path,
            'include_manifest_path': self._host.filesystem.join(metadata_path, *test_manifest_file),
            'manifest_path': self._host.filesystem.join(metadata_path, "MANIFEST.json")}

    def run(self, args):
        if not self.prepare_wpt_checkout():
            return False

        wpt_run_paths = self._wpt_run_paths()
        if not self._options.wpt_metadata:
            # Parse the test expectations JSON and construct corresponding metadata files.
            if not self._generate_metadata_directory(wpt_run_paths["metadata_path"]):
                return False

            # Bail if the include manifest file is not found.
            if not self._host.filesystem.exists(wpt_run_paths["include_manifest_path"]):
                _log.error("Port-specific manifest .ini file does not exist")
                return False
        else:
            # Bail if the specified metadata directory is not found.
            if not self._host.filesystem.exists(wpt_run_paths["metadata_path"]):
                _log.error("Existing wpt metadata directory has to be specified")
                return False

        # Construct the WebDriver instance and run WPT tests via the 'webkit' product.
        webdriver = self._create_webdriver_func(self._port)

        wpt_args = ["run", "--webkit-port={}".format(self._port.name()),
            "--processes={}".format(self._options.child_processes),
            "--metadata={}".format(wpt_run_paths["metadata_path"]),
            "--manifest={}".format(wpt_run_paths["manifest_path"]),
            "--include-manifest={}".format(wpt_run_paths["include_manifest_path"]),
            "--webdriver-binary={}".format(webdriver.binary_path()),
            "--binary={}".format(webdriver.browser_path())]
        wpt_args += ["--binary-arg={}".format(arg) for arg in webdriver.browser_args()]
        wpt_args += ["webkit"] + args

        self._spawn_wpt_func(self._script_name, self._options.wpt_checkout, wpt_args)
        return True
