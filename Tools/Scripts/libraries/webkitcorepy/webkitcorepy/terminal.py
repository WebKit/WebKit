# Copyright (C) 2021, 2022 Apple Inc. All rights reserved.
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

import contextlib
import io
import sys

if not sys.platform.startswith('win'):
    import readline

from webkitcorepy import StringIO, run, Timer

if sys.version_info > (3, 0):
    file = io.IOBase


class Terminal(object):
    _atty_overrides = {}
    colors = True
    URL_PREFIXES = ('file://', 'http://', 'https://', 'radar://', 'rdar://')
    RING_INTERVAL = 30

    @classmethod
    def input(cls, *args, **kwargs):
        alert_after = kwargs.pop('alert_after', None)

        try:
            if alert_after and cls.isatty(sys.stdout):
                with Timer(alert_after, lambda: cls.ring(sys.stdout)):
                    return (input if sys.version_info > (3, 0) else raw_input)(*args, **kwargs)
            else:
                return (input if sys.version_info > (3, 0) else raw_input)(*args, **kwargs)
        except KeyboardInterrupt:
            sys.stderr.write('\nUser interrupted program\n')
            sys.exit(1)

    @classmethod
    def ring(cls, file=sys.stdout):
        if file:
            file.write('\a')
            file.flush()

    @classmethod
    def choose(cls, prompt, options=None, default=None, strict=False, numbered=False, alert_after=RING_INTERVAL):
        options = options or ('Yes', 'No')

        response = None
        while response is None:
            if numbered:
                numbered_options = [
                    ('{}) [{}]' if options[i] == default else '{}) {}').format( i + 1, options[i])
                    for i in range(len(options))
                ]
                sys.stdout.write('{}:\n    {}\n'.format(prompt, '\n    '.join(numbered_options)))
            else:
                sys.stdout.write('{} ({})'.format(prompt, '/'.join([
                    '[{}]'.format(option) if option == default else option for option in options
                ])))
            sys.stdout.flush()
            response = cls.input(': ', alert_after=alert_after)

            if numbered and response.isdigit():
                index = int(response) - 1
                if index >= 0 and index < len(options):
                    response = options[index]

            if not strict and len(response) > 0:
                for option in options:
                    if option.lower().startswith(response.lower()):
                        response = option
                        break

            if response not in options:
                if not default:
                    print("'{}' is not an option".format(response))
                response = default

        return response

    @classmethod
    def assert_writeable_stream(cls, target):
        if not isinstance(target, (io.IOBase, file, StringIO)):
            raise ValueError('{} is not an IO object'.format(target))
        if not isinstance(target, StringIO) and not (getattr(target, 'writable', None) and target.writable()) and 'w' not in getattr(target, 'mode', 'r'):
            raise ValueError('{} is an IO object, but is not writable'.format(target))

    @classmethod
    def supports_color(cls, file):
        if not cls.colors:
            return False
        return cls.isatty(file)

    @classmethod
    def isatty(cls, file):
        try:
            return cls._atty_overrides.get(file.fileno(), file.isatty())
        except (io.UnsupportedOperation, AttributeError):
            return cls._atty_overrides.get(str(file), file.isatty())

    @classmethod
    @contextlib.contextmanager
    def override_atty(cls, target, isatty=True):
        if not isinstance(target, (io.IOBase, file, StringIO)):
            raise ValueError('{} is not an IO object'.format(target))

        try:
            key = target.fileno()
        except (io.UnsupportedOperation, AttributeError):
            key = str(target)

        previous = cls._atty_overrides.get(key)
        cls._atty_overrides[key] = isatty

        try:
            yield
        finally:
            if previous is None:
                del cls._atty_overrides[key]
            else:
                cls._atty_overrides[key] = previous

    @classmethod
    def open_url(cls, url, prompt=None, alert_after=RING_INTERVAL):
        if all(not url.startswith(prefix) for prefix in cls.URL_PREFIXES):
            sys.stderr.write("'{}' is not a valid URL\n")
            return False
        if not cls.isatty(sys.stdout):
            return False

        if prompt:
            try:
                cls.input(prompt, alert_after=alert_after)
            except SystemExit:
                sys.stderr.write('User aborted URL open\n')
                return False

        if sys.platform.startswith('win'):
            process = run(['explorer', url])
        else:
            # TODO: Use shutil directly when Python 2.7 is removed
            from whichcraft import which
            if sys.platform.startswith('linux') and which('xdg-open'):
                process = run(['xdg-open', url])
            else:
                process = run(['open', url])
        return True if process.returncode == 0 else False

    class Text(object):
        value = lambda value: '\033[{}m'.format(value)

        reset = value(0)

        styles = [value(1), value(4), value(5), value(8)]
        bold, underline, blink, concealed = styles

        colors = [value(30), value(31), value(32), value(33), value(34), value(35), value(36), value(37)]
        black, red, green, yellow, blue, magenta, cyan, white = colors

        backgroundColors = [value(40), value(41), value(42), value(43), value(44), value(45), value(46), value(47)]
        blackBackground, redBackground, greenBackground, yellowBackground, blueBackground, magentaBackground, cyanBackground, whiteBackground = colors

    class Style(object):
        top = {}
        _disabled = set()
        _is_styled = set()

        @classmethod
        def enabled(cls, file):
            Terminal.assert_writeable_stream(file)

            try:
                if not Terminal.supports_color(file) or not file.fileno():
                    return False
            except (io.UnsupportedOperation, AttributeError):
                return False
            return file.fileno() not in cls._disabled

        @classmethod
        def disable(cls, file):
            Terminal.assert_writeable_stream(file)
            if not cls.enabled(file):
                return
            cls._disabled.add(file.fileno())
            Terminal.Style().set(file)

        @classmethod
        def enable(cls, file):
            Terminal.assert_writeable_stream(file)
            if cls.enabled(file):
                return

            cls._disabled.discard(file.fileno())
            if cls.top.get(file.fileno()):
                cls.top.get(file.fileno()).set(file)

        @classmethod
        def is_styled(cls, file):
            try:
                return file.fileno() in cls._is_styled
            except (io.UnsupportedOperation, AttributeError):
                return False

        def __init__(self, style=None, color=None, backgroundColor=None):
            if style and style not in Terminal.Text.styles:
                raise ValueError('{} is not a recognized terminal text style'.format(style))
            self.style = style

            if color and color not in Terminal.Text.colors:
                raise ValueError('{} is not a recognized terminal color'.format(color))
            self.color = color

            if backgroundColor and backgroundColor not in Terminal.Text.backgroundColors:
                raise ValueError('{} is not a recognized terminal background color'.format(backgroundColor))
            self.backgroundColor = backgroundColor

        def __repr__(self):
            result = Terminal.Text.reset
            if self.style:
                result += self.style
            if self.color:
                result += self.color
            if self.backgroundColor:
                result += self.backgroundColor
            return result

        def set(self, file):
            Terminal.assert_writeable_stream(file)
            will_style = True
            if not self.style and not self.color and not self.backgroundColor:
                will_style = False
            elif not self.enabled(file):
                will_style = False

            if will_style:
                self._is_styled.add(file.fileno())
                file.write(str(self))
                return

            if self.is_styled(file):
                file.write(Terminal.Text.reset)
            try:
                self._is_styled.discard(file.fileno())
            except (io.UnsupportedOperation, AttributeError):
                pass

        @contextlib.contextmanager
        def apply(self, target):
            Terminal.assert_writeable_stream(target)
            try:
                previous = self.top.get(target.fileno())
            except (io.UnsupportedOperation, AttributeError):
                try:
                    yield
                finally:
                    return

            self.top[target.fileno()] = self
            self.set(target)

            try:
                yield
            finally:
                if previous:
                    previous.set(target)
                    self.top[target.fileno()] = previous
                else:
                    Terminal.Style().set(target)
                    del self.top[target.fileno()]
