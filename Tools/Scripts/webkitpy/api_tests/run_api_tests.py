# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

from __future__ import print_function
import logging
import optparse
import sys
import traceback

from webkitpy.api_tests.manager import Manager
from webkitpy.common.host import Host
from webkitpy.layout_tests.views.metered_stream import MeteredStream
from webkitpy.port import configuration_options, platform_options

EXCEPTIONAL_EXIT_STATUS = -1
INTERRUPT_EXIT_STATUS = -2

_log = logging.getLogger(__name__)


def main(argv, stdout, stderr):
    options, args = parse_args(argv)
    host = Host()

    try:
        options.webkit_test_runner = True
        port = host.port_factory.get(options.platform, options)
    except NotImplementedError as e:
        print(str(e), file=stderr)
        return EXCEPTIONAL_EXIT_STATUS

    # Some platforms do not support API tests
    does_not_support_api_tests = ['ios-device']
    if port.operating_system() in does_not_support_api_tests:
        print('{} cannot run API tests'.format(port.operating_system()), file=stderr)
        return EXCEPTIONAL_EXIT_STATUS

    try:
        return run(port, options, args, stderr)
    except KeyboardInterrupt:
        return INTERRUPT_EXIT_STATUS
    except BaseException as e:
        if isinstance(e, Exception):
            print('\n%s raised: %s' % (e.__class__.__name__, str(e)), file=stderr)
            traceback.print_exc(file=stderr)
        return EXCEPTIONAL_EXIT_STATUS


def run(port, options, args, logging_stream):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG if options.verbose else logging.ERROR if options.quiet else logging.INFO)

    try:
        stream = MeteredStream(logging_stream, options.verbose, logger=logger, number_of_columns=port.host.platform.terminal_width())
        manager = Manager(port, options, stream)

        result = manager.run(args)
        _log.debug("Testing completed, Exit status: %d" % result)
        return result
    finally:
        stream.cleanup()


def parse_args(args):
    option_group_definitions = []

    option_group_definitions.append(('Platform options', platform_options()))
    option_group_definitions.append(('Configuration options', configuration_options()))
    option_group_definitions.append(('Printing Options', [
        optparse.make_option('-q', '--quiet', action='store_true', default=False,
                             help='Run quietly (errors, warnings, and progress only)'),
        optparse.make_option('-v', '--verbose', action='store_true', default=False,
                             help='Enable verbose printing'),
    ]))

    option_group_definitions.append(('WebKit Options', [
        optparse.make_option('-g', '--guard-malloc', action='store_true', default=False,
                             help='Enable Guard Malloc (OS X only)'),
        optparse.make_option('--root', action='store',
                             help='Path to a directory containing the executables needed to run tests.'),
    ]))

    option_group_definitions.append(('Testing Options', [
        optparse.make_option('-d', '--dump', action='store_true', default=False,
                             help='Dump all test names without running them'),
        optparse.make_option('--build', dest='build', action='store_true', default=True,
                             help='Check to ensure the build is up-to-date (default).'),
        optparse.make_option('--no-build', dest='build', action='store_false',
                             help="Don't check to see if the build is up-to-date."),
        optparse.make_option('--timeout', default=30,
                             help='Number of seconds to wait before a test times out'),
        optparse.make_option('--no-timeout', dest='timeout', action='store_false',
                             help='Disable timeouts for all tests'),

        # FIXME: Remove the default, API tests should be multiprocess
        optparse.make_option('--child-processes', default=1,
                             help='Number of processes to run in parallel.'),

        # FIXME: Default should be false, API tests should not be forced to run singly
        optparse.make_option('--run-singly', action='store_true', default=True,
                             help='Run a separate process for each test'),
    ]))

    option_parser = optparse.OptionParser(usage='%prog [options] [<path>...]')

    for group_name, group_options in option_group_definitions:
        option_group = optparse.OptionGroup(option_parser, group_name)
        option_group.add_options(group_options)
        option_parser.add_option_group(option_group)

    return option_parser.parse_args(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], sys.stdout, sys.stderr))
