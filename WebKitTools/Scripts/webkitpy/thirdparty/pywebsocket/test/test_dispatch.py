#!/usr/bin/env python
#
# Copyright 2009, Google Inc.
# All rights reserved.
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


"""Tests for dispatch module."""



import os
import unittest

import config  # This must be imported before mod_pywebsocket.
from mod_pywebsocket import dispatch

import mock


_TEST_HANDLERS_DIR = os.path.join(
        os.path.split(__file__)[0], 'testdata', 'handlers')

_TEST_HANDLERS_SUB_DIR = os.path.join(_TEST_HANDLERS_DIR, 'sub')

class DispatcherTest(unittest.TestCase):
    def test_normalize_path(self):
        self.assertEqual(os.path.abspath('/a/b').replace('\\', '/'),
                         dispatch._normalize_path('/a/b'))
        self.assertEqual(os.path.abspath('/a/b').replace('\\', '/'),
                         dispatch._normalize_path('\\a\\b'))
        self.assertEqual(os.path.abspath('/a/b').replace('\\', '/'),
                         dispatch._normalize_path('/a/c/../b'))
        self.assertEqual(os.path.abspath('abc').replace('\\', '/'),
                         dispatch._normalize_path('abc'))

    def test_converter(self):
        converter = dispatch._path_to_resource_converter('/a/b')
        self.assertEqual('/h', converter('/a/b/h_wsh.py'))
        self.assertEqual('/c/h', converter('/a/b/c/h_wsh.py'))
        self.assertEqual(None, converter('/a/b/h.py'))
        self.assertEqual(None, converter('a/b/h_wsh.py'))

        converter = dispatch._path_to_resource_converter('a/b')
        self.assertEqual('/h', converter('a/b/h_wsh.py'))

        converter = dispatch._path_to_resource_converter('/a/b///')
        self.assertEqual('/h', converter('/a/b/h_wsh.py'))
        self.assertEqual('/h', converter('/a/b/../b/h_wsh.py'))

        converter = dispatch._path_to_resource_converter('/a/../a/b/../b/')
        self.assertEqual('/h', converter('/a/b/h_wsh.py'))

        converter = dispatch._path_to_resource_converter(r'\a\b')
        self.assertEqual('/h', converter(r'\a\b\h_wsh.py'))
        self.assertEqual('/h', converter(r'/a/b/h_wsh.py'))

    def test_source_file_paths(self):
        paths = list(dispatch._source_file_paths(_TEST_HANDLERS_DIR))
        paths.sort()
        self.assertEqual(7, len(paths))
        expected_paths = [
                os.path.join(_TEST_HANDLERS_DIR, 'blank_wsh.py'),
                os.path.join(_TEST_HANDLERS_DIR, 'origin_check_wsh.py'),
                os.path.join(_TEST_HANDLERS_DIR, 'sub',
                             'exception_in_transfer_wsh.py'),
                os.path.join(_TEST_HANDLERS_DIR, 'sub', 'non_callable_wsh.py'),
                os.path.join(_TEST_HANDLERS_DIR, 'sub', 'plain_wsh.py'),
                os.path.join(_TEST_HANDLERS_DIR, 'sub',
                             'wrong_handshake_sig_wsh.py'),
                os.path.join(_TEST_HANDLERS_DIR, 'sub',
                             'wrong_transfer_sig_wsh.py'),
                ]
        for expected, actual in zip(expected_paths, paths):
            self.assertEqual(expected, actual)

    def test_source(self):
        self.assertRaises(dispatch.DispatchError, dispatch._source, '')
        self.assertRaises(dispatch.DispatchError, dispatch._source, 'def')
        self.assertRaises(dispatch.DispatchError, dispatch._source, '1/0')
        self.failUnless(dispatch._source(
                'def web_socket_do_extra_handshake(request):pass\n'
                'def web_socket_transfer_data(request):pass\n'))

    def test_source_warnings(self):
        dispatcher = dispatch.Dispatcher(_TEST_HANDLERS_DIR, None)
        warnings = dispatcher.source_warnings()
        warnings.sort()
        expected_warnings = [
                (os.path.join(_TEST_HANDLERS_DIR, 'blank_wsh.py') +
                 ': web_socket_do_extra_handshake is not defined.'),
                (os.path.join(_TEST_HANDLERS_DIR, 'sub',
                              'non_callable_wsh.py') +
                 ': web_socket_do_extra_handshake is not callable.'),
                (os.path.join(_TEST_HANDLERS_DIR, 'sub',
                              'wrong_handshake_sig_wsh.py') +
                 ': web_socket_do_extra_handshake is not defined.'),
                (os.path.join(_TEST_HANDLERS_DIR, 'sub',
                              'wrong_transfer_sig_wsh.py') +
                 ': web_socket_transfer_data is not defined.'),
                ]
        self.assertEquals(4, len(warnings))
        for expected, actual in zip(expected_warnings, warnings):
            self.assertEquals(expected, actual)

    def test_do_extra_handshake(self):
        dispatcher = dispatch.Dispatcher(_TEST_HANDLERS_DIR, None)
        request = mock.MockRequest()
        request.ws_resource = '/origin_check'
        request.ws_origin = 'http://example.com'
        dispatcher.do_extra_handshake(request)  # Must not raise exception.

        request.ws_origin = 'http://bad.example.com'
        self.assertRaises(Exception, dispatcher.do_extra_handshake, request)

    def test_transfer_data(self):
        dispatcher = dispatch.Dispatcher(_TEST_HANDLERS_DIR, None)
        request = mock.MockRequest(connection=mock.MockConn(''))
        request.ws_resource = '/origin_check'
        request.ws_protocol = 'p1'

        dispatcher.transfer_data(request)
        self.assertEqual('origin_check_wsh.py is called for /origin_check, p1',
                         request.connection.written_data())

        request = mock.MockRequest(connection=mock.MockConn(''))
        request.ws_resource = '/sub/plain'
        request.ws_protocol = None
        dispatcher.transfer_data(request)
        self.assertEqual('sub/plain_wsh.py is called for /sub/plain, None',
                         request.connection.written_data())

        request = mock.MockRequest(connection=mock.MockConn(''))
        request.ws_resource = '/sub/plain?'
        request.ws_protocol = None
        dispatcher.transfer_data(request)
        self.assertEqual('sub/plain_wsh.py is called for /sub/plain?, None',
                         request.connection.written_data())

        request = mock.MockRequest(connection=mock.MockConn(''))
        request.ws_resource = '/sub/plain?q=v'
        request.ws_protocol = None
        dispatcher.transfer_data(request)
        self.assertEqual('sub/plain_wsh.py is called for /sub/plain?q=v, None',
                         request.connection.written_data())

    def test_transfer_data_no_handler(self):
        dispatcher = dispatch.Dispatcher(_TEST_HANDLERS_DIR, None)
        for resource in ['/blank', '/sub/non_callable',
                         '/sub/no_wsh_at_the_end', '/does/not/exist']:
            request = mock.MockRequest(connection=mock.MockConn(''))
            request.ws_resource = resource
            request.ws_protocol = 'p2'
            try:
                dispatcher.transfer_data(request)
                self.fail()
            except dispatch.DispatchError, e:
                self.failUnless(str(e).find('No handler') != -1)
            except Exception:
                self.fail()

    def test_transfer_data_handler_exception(self):
        dispatcher = dispatch.Dispatcher(_TEST_HANDLERS_DIR, None)
        request = mock.MockRequest(connection=mock.MockConn(''))
        request.ws_resource = '/sub/exception_in_transfer'
        request.ws_protocol = 'p3'
        try:
            dispatcher.transfer_data(request)
            self.fail()
        except Exception, e:
            self.failUnless(str(e).find('Intentional') != -1)

    def test_scan_dir(self):
        disp = dispatch.Dispatcher(_TEST_HANDLERS_DIR, None)
        self.assertEqual(3, len(disp._handlers))
        self.failUnless(disp._handlers.has_key('/origin_check'))
        self.failUnless(disp._handlers.has_key('/sub/exception_in_transfer'))
        self.failUnless(disp._handlers.has_key('/sub/plain'))

    def test_scan_sub_dir(self):
        disp = dispatch.Dispatcher(_TEST_HANDLERS_DIR, _TEST_HANDLERS_SUB_DIR)
        self.assertEqual(2, len(disp._handlers))
        self.failIf(disp._handlers.has_key('/origin_check'))
        self.failUnless(disp._handlers.has_key('/sub/exception_in_transfer'))
        self.failUnless(disp._handlers.has_key('/sub/plain'))

    def test_scan_sub_dir_as_root(self):
        disp = dispatch.Dispatcher(_TEST_HANDLERS_SUB_DIR,
                                   _TEST_HANDLERS_SUB_DIR)
        self.assertEqual(2, len(disp._handlers))
        self.failIf(disp._handlers.has_key('/origin_check'))
        self.failIf(disp._handlers.has_key('/sub/exception_in_transfer'))
        self.failIf(disp._handlers.has_key('/sub/plain'))
        self.failUnless(disp._handlers.has_key('/exception_in_transfer'))
        self.failUnless(disp._handlers.has_key('/plain'))

    def test_scan_dir_must_under_root(self):
        dispatch.Dispatcher('a/b', 'a/b/c')  # OK
        dispatch.Dispatcher('a/b///', 'a/b')  # OK
        self.assertRaises(dispatch.DispatchError,
                          dispatch.Dispatcher, 'a/b/c', 'a/b')

    def test_resource_path_alias(self):
        disp = dispatch.Dispatcher(_TEST_HANDLERS_DIR, None)
        disp.add_resource_path_alias('/', '/origin_check')
        self.assertEqual(4, len(disp._handlers))
        self.failUnless(disp._handlers.has_key('/origin_check'))
        self.failUnless(disp._handlers.has_key('/sub/exception_in_transfer'))
        self.failUnless(disp._handlers.has_key('/sub/plain'))
        self.failUnless(disp._handlers.has_key('/'))
        self.assertRaises(dispatch.DispatchError,
                          disp.add_resource_path_alias, '/alias', '/not-exist')


if __name__ == '__main__':
    unittest.main()


# vi:sts=4 sw=4 et
