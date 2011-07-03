# Copyright (c) 2010 Google Inc. All rights reserved.
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

"""Starts a local HTTP server which displays layout test failures (given a test
results directory), provides comparisons of expected and actual results (both
images and text) and allows one-click rebaselining of tests."""

import os
import os.path
import threading

from optparse import make_option

from webkitpy.common import system
from webkitpy.common.net import resultsjsonparser
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.port import factory
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand
from webkitpy.tool.servers.rebaselineserver import RebaselineHTTPServer, STATE_NEEDS_REBASELINE


class TestConfig(object):
    def __init__(self, test_port, layout_tests_directory, results_directory, platforms, filesystem, scm):
        self.test_port = test_port
        self.layout_tests_directory = layout_tests_directory
        self.results_directory = results_directory
        self.platforms = platforms
        self.filesystem = filesystem
        self.scm = scm


class RebaselineServer(AbstractDeclarativeCommand):
    name = "rebaseline-server"
    help_text = __doc__
    argument_names = "/path/to/results/directory"

    def __init__(self):
        options = [
            make_option("--httpd-port", action="store", type="int", default=8127, help="Port to use for the the rebaseline HTTP server"),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    def execute(self, options, args, tool):
        results_directory = args[0]
        filesystem = system.filesystem.FileSystem()
        scm = self._tool.scm()

        if options.dry_run:

            def no_op_copyfile(src, dest):
                pass

            def no_op_add(path, return_exit_code=False):
                if return_exit_code:
                    return 0

            filesystem.copyfile = no_op_copyfile
            scm.add = no_op_add

        print 'Parsing unexpected_results.json...'
        results_json_path = filesystem.join(results_directory, 'unexpected_results.json')
        results_json = json_results_generator.load_json(filesystem, results_json_path)

        port = factory.get()
        layout_tests_directory = port.layout_tests_dir()
        platforms = filesystem.listdir(
            filesystem.join(layout_tests_directory, 'platform'))
        test_config = TestConfig(
            port,
            layout_tests_directory,
            results_directory,
            platforms,
            filesystem,
            scm)

        print 'Gathering current baselines...'
        # Rebaseline server and it's associated JavaScript expected the tests subtree to
        # be key-value pairs instead of hierarchical.
        # FIXME: make the rebaseline server use the hierarchical tree.
        new_tests_subtree = {}

        def gather_baselines(test, result):
            result['state'] = STATE_NEEDS_REBASELINE
            result['baselines'] = _get_test_baselines(test, test_config)
            new_tests_subtree[test] = result

        resultsjsonparser.for_each_test(results_json['tests'], gather_baselines)
        results_json['tests'] = new_tests_subtree

        server_url = "http://localhost:%d/" % options.httpd_port
        print "Starting server at %s" % server_url
        print ("Use the 'Exit' link in the UI, %squitquitquit "
            "or Ctrl-C to stop") % server_url

        threading.Timer(
            .1, lambda: self._tool.user.open_url(server_url)).start()

        httpd = RebaselineHTTPServer(
            httpd_port=options.httpd_port,
            test_config=test_config,
            results_json=results_json,
            platforms_json={
                'platforms': platforms,
                'defaultPlatform': port.name(),
            })
        httpd.serve_forever()
