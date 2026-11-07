# Copyright (C) 2012 Google Inc. All rights reserved.
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

import os
import socket
import subprocess
import unittest

from unittest import mock

from webkitpy.common.host_mock import MockHost
from webkitpy.port import test
from webkitpy.layout_tests.servers.http_server_base import HttpServerBase, ServerError


class TestHttpServerBase(unittest.TestCase):
    def test_corrupt_pid_file(self):
        # This tests that if the pid file is corrupt or invalid,
        # both start() and stop() deal with it correctly and delete the file.
        host = MockHost()
        test_port = test.TestPort(host)

        server = HttpServerBase(test_port)
        server._pid_file = '/tmp/pidfile'
        server._spawn_process = lambda: 4
        server._is_server_running_on_all_ports = lambda: True

        host.filesystem.write_text_file(server._pid_file, 'foo')
        server.stop()
        self.assertEqual(host.filesystem.files[server._pid_file], None)

        host.filesystem.write_text_file(server._pid_file, 'foo')
        server.start()
        self.assertEqual(server._pid, 4)

        # Note that the pid file would not be None if _spawn_process()
        # was actually a real implementation.
        self.assertEqual(host.filesystem.files[server._pid_file], None)

    def test_build_alias_path_pairs(self):
        host = MockHost()
        test_port = test.TestPort(host)
        server = HttpServerBase(test_port)

        data = [
            ['/media-resources', 'media'],
            ['/modern-media-controls', '../Source/WebCore/Modules/modern-media-controls'],
            ['/resources/testharness.css', 'resources/testharness.css'],
        ]

        expected = [
            ('/media-resources', '/test.checkout/LayoutTests/media'),
            ('/modern-media-controls', '/test.checkout/LayoutTests/../Source/WebCore/Modules/modern-media-controls'),
            ('/resources/testharness.css', '/test.checkout/LayoutTests/resources/testharness.css'),
        ]

        self.assertEqual(server._build_alias_path_pairs(data), expected)

    def test_is_udp_port_listening(self):
        held = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            try:
                held.bind(('127.0.0.1', 0))
            except (PermissionError, OSError) as e:
                self.skipTest("Environment does not permit binding a UDP socket: %s" % e)
            held_port = held.getsockname()[1]

            probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            probe.bind(('127.0.0.1', 0))
            free_port = probe.getsockname()[1]
            probe.close()

            self.assertFalse(HttpServerBase._is_udp_port_listening(free_port))
            self.assertTrue(HttpServerBase._is_udp_port_listening(held_port))
            with mock.patch.object(HttpServerBase, 'processes_holding_port',
                                   return_value=[(str(os.getpid()), 'python3')]):
                with mock.patch.object(HttpServerBase, '_parent_pids', return_value={}):
                    self.assertTrue(HttpServerBase._is_udp_port_listening(held_port, owner_pid=os.getpid()))
        finally:
            held.close()

    def test_a_udp_port_held_by_someone_else_is_not_our_server(self):
        with mock.patch.object(HttpServerBase, 'processes_holding_port',
                               return_value=[('4242', 'wpt')]):
            with mock.patch.object(HttpServerBase, '_parent_pids', return_value={4242: 1}):
                held = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                try:
                    try:
                        held.bind(('127.0.0.1', 0))
                    except (PermissionError, OSError) as e:
                        self.skipTest("Environment does not permit binding a UDP socket: %s" % e)
                    port = held.getsockname()[1]
                    self.assertFalse(HttpServerBase._is_udp_port_listening(port, owner_pid=99999))
                finally:
                    held.close()

    def test_a_holder_we_cannot_name_is_still_treated_as_our_server(self):
        held = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            try:
                held.bind(('127.0.0.1', 0))
            except (PermissionError, OSError) as e:
                self.skipTest("Environment does not permit binding a UDP socket: %s" % e)
            port = held.getsockname()[1]
            with mock.patch.object(HttpServerBase, 'processes_holding_port', return_value=[]):
                self.assertTrue(HttpServerBase._is_udp_port_listening(port, owner_pid=99999))
            with mock.patch.object(HttpServerBase, 'processes_holding_port',
                                   return_value=[('4242', 'wpt')]):
                with mock.patch.object(HttpServerBase, '_parent_pids', return_value=None):
                    self.assertTrue(HttpServerBase._is_udp_port_listening(port, owner_pid=99999))
        finally:
            held.close()

    def test_a_pid_is_its_own_descendant_and_a_stranger_is_not(self):
        self.assertTrue(HttpServerBase._is_pid_or_descendant(os.getpid(), os.getpid(), {}))
        self.assertIsNone(HttpServerBase._is_pid_or_descendant('not-a-pid', os.getpid(), {}))

        parents = {400: 300, 300: 200, 200: 1}
        self.assertTrue(HttpServerBase._is_pid_or_descendant('400', '200', parents))
        self.assertFalse(HttpServerBase._is_pid_or_descendant('200', '400', parents))
        self.assertFalse(HttpServerBase._is_pid_or_descendant('999', '200', parents))

    def test_pid_ancestry_says_it_cannot_tell_rather_than_no(self):
        with mock.patch('subprocess.run', side_effect=OSError('no ps')):
            self.assertIsNone(HttpServerBase._parent_pids())
        self.assertIsNone(HttpServerBase._is_pid_or_descendant('400', '200', None))

    def test_parent_pids_reads_the_whole_table_in_one_call(self):
        completed = subprocess.CompletedProcess([], 0, stdout='  400   300\n  300     1\nbogus\n')
        with mock.patch('subprocess.run', return_value=completed) as run:
            self.assertEqual(HttpServerBase._parent_pids(), {400: 300, 300: 1})
        self.assertEqual(run.call_count, 1)

    def test_a_udp_mapping_is_probed_with_a_udp_socket(self):
        host = MockHost()
        server = HttpServerBase(test.TestPort(host))
        held = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            try:
                held.bind(('127.0.0.1', 0))
            except (PermissionError, OSError) as e:
                self.skipTest("Environment does not permit binding a UDP socket: %s" % e)
            port = held.getsockname()[1]

            server._mappings = [{'port': port, 'udp': True}]
            with mock.patch.object(HttpServerBase, 'processes_holding_port', return_value=[]):
                with self.assertRaises(ServerError) as raised:
                    server._check_that_all_ports_are_available()
            self.assertIn('UDP port %d is already in use' % port, str(raised.exception))
        finally:
            held.close()

    def test_a_port_in_use_message_names_the_holder(self):
        with mock.patch.object(HttpServerBase, 'processes_holding_port',
                               return_value=[('4242', 'httpd')]):
            message = HttpServerBase.port_in_use_message(8000)
        self.assertEqual(message, 'TCP port 8000 is already in use. Held by httpd (pid 4242).')

    def test_a_port_in_use_message_without_a_known_holder(self):
        with mock.patch.object(HttpServerBase, 'processes_holding_port', return_value=[]):
            message = HttpServerBase.port_in_use_message(8053, is_udp=True)
        self.assertEqual(message, 'UDP port 8053 is already in use.')

    def test_processes_holding_port_parses_lsof_field_output(self):
        completed = subprocess.CompletedProcess([], 0, stdout='p4242\nchttpd\np4243\ncpython3\n')
        with mock.patch('shutil.which', return_value='/usr/sbin/lsof'):
            with mock.patch('subprocess.run', return_value=completed) as run:
                self.assertEqual(HttpServerBase.processes_holding_port(8000),
                                 [('4242', 'httpd'), ('4243', 'python3')])
        self.assertEqual(run.call_args[0][0], ['/usr/sbin/lsof', '-nP', '-F', 'pc', '-iTCP:8000', '-sTCP:LISTEN'])

    def test_processes_holding_port_never_raises(self):
        with mock.patch('shutil.which', return_value='/usr/sbin/lsof'):
            with mock.patch('subprocess.run', side_effect=OSError('no lsof')):
                self.assertEqual(HttpServerBase.processes_holding_port(8000), [])
        with mock.patch('shutil.which', return_value=None):
            self.assertEqual(HttpServerBase.processes_holding_port(8000), [])
