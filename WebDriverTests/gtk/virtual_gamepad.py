#!/usr/bin/env python3
# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

"""Integration tests for the GTK virtual-gamepad feasibility prototype.

Start the modified WebKitWebDriver with --port=9515 under an X11/Wayland
display, then run:

    python3 WebDriverTests/gtk/virtual_gamepad.py --url http://127.0.0.1:9515

Pass --browser /absolute/path/to/MiniBrowser when the driver cannot locate it.
This suite uses only the Python standard library and creates its own localhost
HTTP server. It is intentionally outside imported WPT: the vendor extension
and proposed WPT builders have not been standardized or upstreamed.

Prototype scope: one top-level GTK browsing context, normalized axes/buttons,
and input-based visibility. The provider replaces hardware input for the UI
process while its automation session owns it. Names and commands are scoped
to the session, but page-level isolation across multiple windows/process pools
is not implemented; use a dedicated MiniBrowser process with a single window.
Independent button sensors, touch, haptics, and non-normalized bounds reject.
"""

import argparse
import http.server
import json
import threading
import time
import unittest
import urllib.error
import urllib.request


class PageHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b'<!doctype html><title>Virtual gamepad</title><input id="target">'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


class VirtualGamepadTests(unittest.TestCase):
    driver_url = 'http://127.0.0.1:9515'
    browser = None

    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), PageHandler)
        cls.server_thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.server_thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.server_thread.join()

    def request(self, method, path, body=None):
        data = None if body is None else json.dumps(body).encode()
        request = urllib.request.Request(self.driver_url + path, data=data, method=method,
                                        headers={'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                return response.status, json.load(response)['value']
        except urllib.error.HTTPError as response:
            return response.code, json.load(response)['value']

    def command(self, method, path, body=None):
        status, value = self.request(method, '/session/' + self.session_id + path, body)
        self.assertEqual(status, 200, value)
        return value

    def new_session(self):
        capabilities = {}
        if self.browser:
            capabilities['webkitgtk:browserOptions'] = {'binary': self.browser}
        status, value = self.request('POST', '/session', {'capabilities': {'alwaysMatch': capabilities}})
        self.assertEqual(status, 200, value)
        self.session_id = value['sessionId']
        self.command('POST', '/url', {'url': 'http://127.0.0.1:%d/' % self.server.server_port})

    def setUp(self):
        self.session_id = None
        self.addCleanup(self.delete_session)
        self.new_session()

    def delete_session(self):
        if self.session_id:
            status, value = self.request('DELETE', '/session/' + self.session_id)
            self.session_id = None
            self.assertEqual(status, 200, value)

    def script(self, script):
        return self.command('POST', '/execute/sync', {'script': script, 'args': []})

    def connect(self):
        # Register before the page starts monitoring, to exercise initial state
        # propagation as well as connections while a provider already has clients.
        result = self.command('POST', '/webkit/gamepad/connect', {
            'id': 'WebKit virtual gamepad test', 'mapping': 'standard',
            'axes': [{'minimum': -1, 'maximum': 1}],
            'buttons': [{'minimum': 0, 'maximum': 1}],
            'surfaces': [], 'vibration': [],
        })
        self.assertIsInstance(result['name'], str)
        self.script('''
            window.gamepadEvents = [];
            window.keyEvents = [];
            addEventListener('gamepadconnected', e => gamepadEvents.push(['connected', e.gamepad.id]));
            addEventListener('gamepaddisconnected', e => gamepadEvents.push(['disconnected', e.gamepad.id]));
            addEventListener('keydown', e => keyEvents.push(e.type));
            addEventListener('keyup', e => keyEvents.push(e.type));
            document.querySelector('input').focus();
            navigator.getGamepads();
        ''')
        return result['name']

    def actions(self, name, items, other_sources=()):
        return self.command('POST', '/actions', {
            'actions': [{'type': 'gamepad', 'id': name, 'actions': items}, *other_sources]})

    def axis(self, value, index=0):
        return {'type': 'gamepadAxisInput', 'axisIndex': index, 'value': value}

    def button(self, value):
        return {'type': 'gamepadButtonInput', 'buttonIndex': 0, 'value': value}

    def snapshot(self):
        return self.script('''
            return Array.from(navigator.getGamepads()).filter(Boolean).map(p => ({
                id: p.id, axes: Array.from(p.axes), timestamp: p.timestamp,
                buttons: p.buttons.map(b => ({value: b.value, pressed: b.pressed}))
            }));
        ''')

    def wait_for(self, predicate):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            time.sleep(0.02)
        self.fail('Timed out waiting for renderer-visible gamepad state')

    def wait_for_axis(self, value):
        return self.wait_for(lambda: next((p for p in self.snapshot() if p['axes'] == [value]), None))

    def assert_error(self, method, path, body, error):
        status, value = self.request(method, '/session/' + self.session_id + path, body)
        self.assertNotEqual(status, 200, value)
        self.assertEqual(value['error'], error, value)

    def test_connection_values_timestamps_and_disconnection(self):
        name = self.connect()
        self.assertEqual(self.snapshot(), [])
        self.actions(name, [self.axis(0.5), self.button(1)])
        first = self.wait_for_axis(0.5)
        self.wait_for(lambda: self.snapshot()[0]['buttons'][0]['pressed'])
        self.wait_for(lambda: self.script('return gamepadEvents.some(e => e[0] === "connected")'))
        self.actions(name, [self.axis(-0.25)])
        second = self.wait_for_axis(-0.25)
        self.assertGreater(second['timestamp'], first['timestamp'])
        self.command('POST', '/webkit/gamepad/disconnect', {'name': name})
        self.wait_for(lambda: self.script('return gamepadEvents.some(e => e[0] === "disconnected")'))
        self.assertEqual(self.snapshot(), [])

    def test_release_resets_without_disconnecting(self):
        name = self.connect()
        self.actions(name, [self.axis(0.5), self.button(1)])
        self.wait_for_axis(0.5)
        self.command('DELETE', '/actions')
        gamepad = self.wait_for_axis(0)
        self.assertEqual(gamepad['buttons'], [{'value': 0, 'pressed': False}])
        self.actions(name, [self.axis(-0.5)])
        self.wait_for_axis(-0.5)

    def test_mixed_actions_and_pause(self):
        name = self.connect()
        self.actions(name, [self.axis(0.5), {'type': 'pause', 'duration': 50}, self.axis(-0.5)], [{
            'type': 'key', 'id': 'keyboard', 'actions': [
                {'type': 'keyDown', 'value': 'x'}, {'type': 'keyUp', 'value': 'x'}, {'type': 'pause'}],
        }])
        self.wait_for_axis(-0.5)
        self.assertEqual(self.script('return keyEvents'), ['keydown', 'keyup'])

    def test_invalid_later_tick_does_not_apply_earlier_tick(self):
        name = self.connect()
        self.actions(name, [self.axis(0.25)])
        self.wait_for_axis(0.25)
        self.assert_error('POST', '/actions', {'actions': [{
            'type': 'gamepad', 'id': name, 'actions': [self.axis(0.5), self.axis(0.75, index=1)],
        }]}, 'invalid argument')
        self.assertEqual(self.snapshot()[0]['axes'], [0.25])

    def test_stale_names_and_out_of_range_values(self):
        name = self.connect()
        for action in [self.axis(2), self.axis(0, index=-1), self.axis(0, index=True)]:
            self.assert_error('POST', '/actions', {'actions': [{
                'type': 'gamepad', 'id': name, 'actions': [action],
            }]}, 'invalid argument')
        self.command('POST', '/webkit/gamepad/disconnect', {'name': name})
        self.assert_error('POST', '/webkit/gamepad/disconnect', {'name': name}, 'invalid argument')
        self.assert_error('POST', '/actions', {'actions': [{
            'type': 'gamepad', 'id': name, 'actions': [{'type': 'pause'}],
        }]}, 'invalid argument')

    def test_unsupported_descriptions(self):
        for options in [{'vibration': ['dual-rumble']}, {'surfaces': [{}]}, {
            'axes': [{'minimum': 0, 'maximum': 255}],
        }, {'buttons': [{'minimum': 0, 'maximum': 1, 'type': 'trackpad'}]}]:
            self.assert_error('POST', '/webkit/gamepad/connect', options, 'unsupported operation')
        self.assert_error('POST', '/webkit/gamepad/connect', {'axes': [False]}, 'invalid argument')

    def test_session_cleanup_and_name_ownership(self):
        old_name = self.connect()
        self.actions(old_name, [self.axis(0.5)])
        self.wait_for_axis(0.5)
        self.delete_session()
        self.new_session()
        self.assert_error('POST', '/webkit/gamepad/disconnect', {'name': old_name}, 'invalid argument')
        name = self.connect()
        self.assertNotEqual(old_name, name)
        self.assertEqual(self.snapshot(), [])
        self.actions(name, [self.axis(-0.5)])
        self.assertEqual(len(self.wait_for(lambda: self.snapshot())), 1)
        self.wait_for_axis(-0.5)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default=VirtualGamepadTests.driver_url)
    parser.add_argument('--browser')
    options, remaining = parser.parse_known_args()
    VirtualGamepadTests.driver_url = options.url.rstrip('/')
    VirtualGamepadTests.browser = options.browser
    unittest.main(argv=['virtual_gamepad.py', *remaining])
