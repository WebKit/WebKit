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

#
# FIXME: this is a poor attempt at a unit tests driver. We should replace
# this with something that actually uses a unit testing framework or
# at least produces output that could be useful.

"""Simple test client for the port/Driver interface."""

import os
import optparse
import port


def run_tests(port, options, tests):
    # |image_path| is a path to the image capture from the driver.
    image_path = 'image_result.png'
    driver = port.create_driver(image_path, None)
    driver.start()
    for t in tests:
        uri = port.filename_to_uri(os.path.join(port.layout_tests_dir(), t))
        print "uri: " + uri
        crash, timeout, checksum, output, err = \
            driver.run_test(uri, int(options.timeout), None)
        print "crash:         " + str(crash)
        print "timeout:       " + str(timeout)
        print "checksum:      " + str(checksum)
        print 'stdout: """'
        print ''.join(output)
        print '"""'
        print 'stderr: """'
        print ''.join(err)
        print '"""'
        print
    driver.stop()


if __name__ == '__main__':
    # FIXME: configuration_options belong in a shared location.
    configuration_options = [
        optparse.make_option('--debug', action='store_const', const='Debug', dest="configuration", help='Set the configuration to Debug'),
        optparse.make_option('--release', action='store_const', const='Release', dest="configuration", help='Set the configuration to Release'),
    ]
    misc_options = [
        optparse.make_option('-p', '--platform', action='store', default='mac', help='Platform to test (e.g., "mac", "chromium-mac", etc.'),
        optparse.make_option('--timeout', action='store', default='2000', help='test timeout in milliseconds (2000 by default)'),
        optparse.make_option('--wrapper', action='store'),
        optparse.make_option('--no-pixel-tests', action='store_true', default=False, help='disable pixel-to-pixel PNG comparisons'),
    ]
    option_list = configuration_options + misc_options
    optparser = optparse.OptionParser(option_list=option_list)
    options, args = optparser.parse_args()
    p = port.get(options.platform, options)
    run_tests(p, options, args)
