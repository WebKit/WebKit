#!/usr/bin/env python

# Copyright (C) 2005, 2006, 2007, 2015 Apple Inc.  All rights reserved.
# Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
# Copyright (C) 2011 Research In Motion Limited. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import print_function
import optparse
import subprocess
import sys
import time

from webkitpy.common.host import Host
from webkitpy.layout_tests.servers import web_platform_test_server
from webkitpy.layout_tests.run_webkit_tests import EXCEPTIONAL_EXIT_STATUS
from webkitpy.port import platform_options


def parse_args(args):
    parser = optparse.OptionParser()
    parser.add_option("-a", "--all-interfaces", help="Bind to all interfaces", action="store_true", dest="http_all_interfaces")
    parser.add_option("-p", "--port", help="Bind to port NNNN", action="store", type="int", dest="http_port")
    parser.add_option("--no-httpd", help="Do not start httpd server", action="store_false", default=True, dest="httpd_server")
    parser.add_option("--no-wpt", help="Do not start web-platform-tests server", action="store_false", default=True, dest="web_platform_test_server")
    parser.add_option("-D", "--additional-dir", help="Additional directory and alias", action="append", default=[], type="string", nargs=2, dest="additional_dirs")
    parser.add_option("-u", "--open-url", help="Open an URL", action="store", default=[], type="string", dest="url")

    option_group = optparse.OptionGroup(parser, "Platform options")
    option_group.add_options(platform_options())
    parser.add_option_group(option_group)

    return parser.parse_args(args)


def main(argv, stdout, stderr):
    options, args = parse_args(argv)
    run_server(options, args, stdout, stderr)


def run_server(options, args, stdout, stderr):
    host = Host()
    with host.filesystem.mkdtemp(prefix='webkit-httpd-') as temporary_directory:
        log_file = host.filesystem.join(temporary_directory, 'access_log')
        run_server_with_log_file(host, options, stdout, stderr, log_file)


def run_server_with_log_file(host, options, stdout, stderr, log_file):
    options.http_access_log = log_file
    options.http_error_log = log_file

    try:
        port = host.port_factory.get(options.platform, options)
    except NotImplementedError as e:
        print(str(e), file=stderr)
        return EXCEPTIONAL_EXIT_STATUS

    if options.web_platform_test_server:
        print("Starting web-platform-tests server on <%s> and <%s>" % (web_platform_test_server.base_http_url(port), web_platform_test_server.base_https_url(port)))
        print("WebKit http/wpt tests are accessible at <%s>" % (web_platform_test_server.base_http_url(port) + "WebKit/"))
        port.start_web_platform_test_server()

    if options.httpd_server:
        # FIXME(154294): somehow retrieve the actual ports and interfaces bound by the httpd server
        http_port = options.http_port if options.http_port is not None else "8000"
        if options.http_all_interfaces is not None:
            print("Starting httpd on port %s (all interfaces)" % http_port)
        else:
            print("Starting httpd on <http://127.0.0.1:%s>" % http_port)

        additionalDirs = {additional_dir[0]: additional_dir[1] for additional_dir in options.additional_dirs}
        port.start_http_server(additionalDirs)
        port.start_websocket_server()

    if options.url:
        print("Opening %s" % options.url)
        subprocess.Popen(['open', options.url], stdout=subprocess.PIPE)

    try:
        for line in follow_file(log_file):
            stdout.write(line)
    except KeyboardInterrupt:
        if options.web_platform_test_server:
            port.stop_web_platform_test_server()
        if options.httpd_server:
            port.stop_http_server()
            port.stop_websocket_server()


def follow_file(path):
    """Python equivalent of TAIL(1)"""
    with open(path) as file_handle:
        while True:
            line = file_handle.readline()
            if not line:
                time.sleep(0.1)
                continue
            yield line


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], sys.stdout, sys.stderr))
