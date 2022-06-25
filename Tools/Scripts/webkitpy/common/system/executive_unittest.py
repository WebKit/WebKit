# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2009 Daniel Bates (dbates@intudata.com). All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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
import signal
import subprocess
import sys
import time
import unittest

# Since we execute this script directly as part of the unit tests, we need to ensure
# that Tools/Scripts is in sys.path for the next imports to work correctly.
script_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)
third_party_py = os.path.join(script_dir, "webkitpy", "thirdparty", "autoinstalled")
if third_party_py not in sys.path:
    sys.path.insert(0, third_party_py)

from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.filesystem_mock import MockFileSystem

from webkitcorepy import string_utils


class ScriptErrorTest(unittest.TestCase):
    def test_message_with_output(self):
        error = ScriptError('My custom message!', '', -1)
        self.assertEqual(error.message_with_output(), 'My custom message!')
        error = ScriptError('My custom message!', '', -1, 'My output.')
        self.assertEqual(error.message_with_output(), 'My custom message!\n\nMy output.')
        error = ScriptError('', 'my_command!', -1, 'My output.', '/Users/username/blah')
        self.assertEqual(error.message_with_output(), 'Failed to run "\'my_command!\'" exit_code: -1 cwd: /Users/username/blah\n\nMy output.')
        error = ScriptError('', 'my_command!', -1, 'ab' + '1' * 499)
        self.assertEqual(error.message_with_output(), 'Failed to run "\'my_command!\'" exit_code: -1\n\nLast 500 characters of output:\nb' + '1' * 499)

    def test_message_with_tuple(self):
        error = ScriptError('', ('my', 'command'), -1, 'My output.', '/Users/username/blah')
        self.assertEqual(error.message_with_output(), 'Failed to run "(\'my\', \'command\')" exit_code: -1 cwd: /Users/username/blah\n\nMy output.')


def never_ending_command():
    """Arguments for a command that will never end (useful for testing process
    killing). It should be a process that is unlikely to already be running
    because all instances will be killed."""
    if sys.platform.startswith('win'):
        return ['wmic']
    return ['yes']


def command_line(cmd, *args):
    return [sys.executable, __file__, '--' + cmd] + list(args)


class ExecutiveTest(unittest.TestCase):
    def assert_interpreter_for_content(self, intepreter, content):
        fs = MockFileSystem()

        tempfile, temp_name = fs.open_binary_tempfile('')
        tempfile.write(content)
        tempfile.close()
        file_interpreter = Executive.interpreter_for_script(temp_name, fs)

        self.assertEqual(file_interpreter, intepreter)

    def test_interpreter_for_script(self):
        self.assert_interpreter_for_content(None, '')
        self.assert_interpreter_for_content(None, 'abcd\nefgh\nijklm')
        self.assert_interpreter_for_content(None, '##/usr/bin/perl')
        self.assert_interpreter_for_content('perl', '#!/usr/bin/env perl')
        self.assert_interpreter_for_content('perl', '#!/usr/bin/env perl\nfirst\nsecond')
        self.assert_interpreter_for_content('perl', '#!/usr/bin/perl')
        self.assert_interpreter_for_content('perl', '#!/usr/bin/perl -w')
        self.assert_interpreter_for_content(sys.executable, '#!/usr/bin/env python')
        self.assert_interpreter_for_content(sys.executable, '#!/usr/bin/env python\nfirst\nsecond')
        self.assert_interpreter_for_content(sys.executable, '#!/usr/bin/python')
        self.assert_interpreter_for_content('ruby', '#!/usr/bin/env ruby')
        self.assert_interpreter_for_content('ruby', '#!/usr/bin/env ruby\nfirst\nsecond')
        self.assert_interpreter_for_content('ruby', '#!/usr/bin/ruby')

    def test_run_command_with_bad_command(self):
        self.assertRaises(OSError, lambda: Executive().run_command(["foo_bar_command_blah"], ignore_errors=True, return_exit_code=True))
        self.assertRaises(OSError, lambda: Executive().run_and_throw_if_fail(["foo_bar_command_blah"], quiet=True))
        self.assertRaises(ScriptError, lambda: Executive().run_command([sys.executable, '-c', 'import sys; sys.exit(1)']))
        self.assertRaises(ScriptError, lambda: Executive().run_and_throw_if_fail([sys.executable, '-c', 'import sys; sys.exit(1)'], quiet=True))

    def test_run_command_args_type(self):
        executive = Executive()
        self.assertRaises(AssertionError, executive.run_command, "echo")
        self.assertRaises(AssertionError, executive.run_command, u"echo")
        executive.run_command(command_line('echo', 'foo'))
        executive.run_command(tuple(command_line('echo', 'foo')))

    def test_auto_stringify_args(self):
        executive = Executive()
        executive.run_command(command_line('echo', 1))
        with executive.popen(command_line('echo', 1), stdout=executive.PIPE) as process:
            process.wait()
            self.assertEqual('echo 1', executive.command_for_printing(['echo', 1]))

    def test_popen_args(self):
        executive = Executive()
        # Explicitly naming the 'args' argument should not thow an exception.
        with executive.popen(args=command_line('echo', 1), stdout=executive.PIPE) as process:
            process.wait()

    def test_run_command_with_unicode(self):
        """Validate that it is safe to pass unicode() objects
        to Executive.run* methods, and they will return unicode()
        objects by default unless decode_output=False"""
        unicode_tor_input = u"WebKit \u2661 Tor Arne Vestb\u00F8!"
        if sys.platform.startswith('win'):
            encoding = 'mbcs'
        else:
            encoding = 'utf-8'
        encoded_tor = string_utils.encode(unicode_tor_input, encoding=encoding)
        # On Windows, we expect the unicode->mbcs->unicode roundtrip to be
        # lossy. On other platforms, we expect a lossless roundtrip.
        if sys.platform.startswith('win'):
            unicode_tor_output = string_utils.decode(encoded_tor, encoding=encoding)
        else:
            unicode_tor_output = unicode_tor_input

        executive = Executive()

        output = executive.run_command(command_line('cat'), input=unicode_tor_input)
        self.assertEqual(output, unicode_tor_output)

        output = executive.run_command(command_line('echo', unicode_tor_input))
        self.assertEqual(output, unicode_tor_output)

        output = executive.run_command(command_line('echo', unicode_tor_input), decode_output=False)
        self.assertEqual(output, encoded_tor)

        # Make sure that str() input also works.
        output = executive.run_command(command_line('cat'), input=encoded_tor, decode_output=False)
        self.assertEqual(output, encoded_tor)

        # FIXME: We should only have one run* method to test
        output = executive.run_and_throw_if_fail(command_line('echo', unicode_tor_input), quiet=True)
        self.assertEqual(output, unicode_tor_output)

        output = executive.run_and_throw_if_fail(command_line('echo', unicode_tor_input), quiet=True, decode_output=False)
        self.assertEqual(output, encoded_tor)

    def serial_test_kill_process(self):
        executive = Executive()
        with executive.popen(never_ending_command(), stdout=subprocess.PIPE) as process:
            self.assertEqual(process.poll(), None)  # Process is running
            executive.kill_process(process.pid)
            # Note: Can't use a ternary since signal.SIGKILL is undefined for sys.platform == "win32"
            if sys.platform.startswith('win'):
                # FIXME: https://bugs.webkit.org/show_bug.cgi?id=54790
                # We seem to get either 0 or 1 here for some reason.
                self.assertIn(process.wait(), (0, 1))
            elif sys.platform == "cygwin":
                # FIXME: https://bugs.webkit.org/show_bug.cgi?id=98196
                # cygwin seems to give us either SIGABRT or SIGKILL
                # Native Windows (via Cygwin) returns ENOTBLK (-15)
                self.assertIn(process.wait(), (-signal.SIGABRT, -signal.SIGKILL, -15))
            else:
                expected_exit_code = -signal.SIGTERM
                self.assertEqual(process.wait(), expected_exit_code)

            # Killing again should fail silently.
            executive.kill_process(process.pid)

    def serial_test_kill_all(self):
        executive = Executive()
        with executive.popen(never_ending_command(), stdout=subprocess.PIPE) as process:
            self.assertIsNone(process.poll())  # Process is running
            executive.kill_all(never_ending_command()[0])
            # Note: Can't use a ternary since signal.SIGTERM is undefined for sys.platform == "win32"
            if sys.platform == "cygwin":
                expected_exit_code = 0  # os.kill results in exit(0) for this process.
                self.assertEqual(process.wait(), expected_exit_code)
            elif sys.platform.startswith('win'):
                # FIXME: https://bugs.webkit.org/show_bug.cgi?id=54790
                # We seem to get either 0 or 1 here for some reason.
                self.assertIn(process.wait(), (0, 1))
            else:
                expected_exit_code = -signal.SIGTERM
                self.assertEqual(process.wait(), expected_exit_code)
            # Killing again should fail silently.
            executive.kill_all(never_ending_command()[0])

    def _assert_windows_image_name(self, name, expected_windows_name):
        executive = Executive()
        windows_name = executive._windows_image_name(name)
        self.assertEqual(windows_name, expected_windows_name)

    def test_windows_image_name(self):
        self._assert_windows_image_name("foo", "foo.exe")
        self._assert_windows_image_name("foo.exe", "foo.exe")
        self._assert_windows_image_name("foo.com", "foo.com")
        # If the name looks like an extension, even if it isn't
        # supposed to, we have no choice but to return the original name.
        self._assert_windows_image_name("foo.baz", "foo.baz")
        self._assert_windows_image_name("foo.baz.exe", "foo.baz.exe")

    def serial_test_check_running_pid(self):
        executive = Executive()
        self.assertTrue(executive.check_running_pid(os.getpid()))
        # Maximum pid number on Linux is 32768 by default
        self.assertFalse(executive.check_running_pid(100000))

    def serial_test_running_pids(self):
        if sys.platform.startswith('win') or sys.platform == "cygwin":
            return  # This function isn't implemented on Windows yet.

        executive = Executive()
        pids = executive.running_pids()
        self.assertIn(os.getpid(), pids)

    def serial_test_run_in_parallel(self):
        # We run this test serially to avoid overloading the machine and throwing off the timing.

        if sys.platform.startswith('win') or sys.platform == "cygwin":
            return  # This function isn't implemented properly on windows yet.
        import multiprocessing

        NUM_PROCESSES = 4
        DELAY_SECS = 1.0  # make sure this is much greater than the VM spawn time
        cmd_line = [sys.executable, '-c', 'import time; time.sleep(%f); print("hello")' % DELAY_SECS]
        cwd = os.getcwd()
        commands = [tuple([cmd_line, cwd])] * NUM_PROCESSES

        try:
            # we overwrite __main__ to be this to avoid any issues with
            # multiprocessing's spawning caused by multiple versions of pytest on
            # sys.path
            old_main = sys.modules["__main__"]
            sys.modules["__main__"] = sys.modules[__name__]
            start = time.time()
            command_outputs = Executive().run_in_parallel(commands, processes=NUM_PROCESSES)
            done = time.time()
        finally:
            sys.modules["__main__"] = old_main

        self.assertTrue(done - start < NUM_PROCESSES * DELAY_SECS)
        self.assertEqual([output[1] for output in command_outputs], [b'hello\n'] * NUM_PROCESSES)
        self.assertEqual([],  multiprocessing.active_children())

    def test_run_in_parallel_assert_nonempty(self):
        self.assertRaises(AssertionError, Executive().run_in_parallel, [])


def main(platform, stdin, stdout, cmd, args):
    if sys.platform.startswith('win') and hasattr(stdout, 'fileno'):
        import msvcrt
        msvcrt.setmode(stdout.fileno(), os.O_BINARY)
    if cmd == '--cat':
        stdout.write(stdin.read())
    elif cmd == '--echo':
        stdout.write(' '.join(args))
    return 0

if __name__ == '__main__' and len(sys.argv) > 1 and sys.argv[1] in ('--cat', '--echo'):
    sys.exit(main(sys.platform, sys.stdin, sys.stdout, sys.argv[1], sys.argv[2:]))
