#!/usr/bin/python
#
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
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
# This is a Python port of Douglas Crockford's jsmin.cc. See original
# copyright notice below.
#
# Copyright (c) 2002 Douglas Crockford  (www.crockford.com)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

from cStringIO import StringIO
import sys


class UnterminatedComment(Exception):
    pass


class UnterminatedStringLiteral(Exception):
    pass


class UnterminatedRegularExpression(Exception):
    pass


EOF = ''


def jsmin(text):
    minifier = JavaScriptMinifier()
    minifier.input = StringIO(text)
    minifier.output = StringIO()
    minifier.jsmin()
    return minifier.output.getvalue()


class JavaScriptMinifier(object):

    def isAlphanum(self, c):
        """ return true if the character is a letter, digit, underscore,
            dollar sign, or non-ASCII character.
        """
        return ((c >= 'a' and c <= 'z') or (c >= '0' and c <= '9') or
            (c >= 'A' and c <= 'Z') or c == '_' or c == '$' or c == '\\' or
            c > 126)

    def get(self):
        """ return the next character from stdin. Watch out for lookahead. If
            the character is a control character, translate it to a space or
            linefeed.
        """
        c = self.theLookahead
        self.theLookahead = EOF
        if c == EOF:
            c = self.input.read(1)
        if c >= ' ' or c == '\n' or c == EOF:
            return c
        if c == '\r':
            return '\n'
        return ' '

    def peek(self):
        """ get the next character without getting it. """
        self.theLookahead = self.get()
        return self.theLookahead

    def next(self):
        """ get the next character, excluding comments. peek() is used to see
            if a '/' is followed by a '/' or '*'.
        """
        c = self.get()
        if c == '/':
            peek = self.peek()
            if peek == '/':
                while True:
                    c = self.get()
                    if c <= '\n':
                        return c
            elif peek == '*':
                self.get()
                while True:
                    get = self.get()
                    if get == '*':
                        if self.peek() == '/':
                            self.get()
                            return ' '
                    elif get == EOF:
                        raise UnterminatedComment()
            else:
                return c
        return c

    def putc(self, c):
        self.output.write(c)

    def action(self, d):
        """ do something! What you do is determined by the argument:
               1   Output A. Copy B to A. Get the next B.
               2   Copy B to A. Get the next B. (Delete A).
               3   Get the next B. (Delete B).
            action treats a string as a single character. Wow!
            action recognizes a regular expression if it is preceded by ( or , or =.
        """
        if d <= 1:
            self.putc(self.theA)
        if d <= 2:
            self.theA = self.theB
            if self.theA == '\'' or self.theA == '"':
                while True:
                    self.putc(self.theA)
                    self.theA = self.get()
                    if self.theA == self.theB:
                        break
                    if self.theA == '\\':
                        self.putc(self.theA)
                        self.theA = self.get()
                    if self.theA == EOF:
                        raise UnterminatedString()
        if d <= 3:
            self.theB = self.next()
            if self.theB == '/' and self.theA in ['(', ',', '=', ':', '[', '!', '&', '|', '?', '{', '}', ';', '\n']:
                self.putc(self.theA)
                self.putc(self.theB)
                while True:
                    self.theA = self.get()
                    if self.theA == '/':
                        break
                    if self.theA == '\\':
                        self.putc(self.theA)
                        self.theA = self.get()
                    if self.theA == EOF:
                        raise UnterminatedRegularExpression()
                    self.putc(self.theA)
                self.theB = self.next()

    def jsmin(self):
        """ Copy the input to the output, deleting the characters which are
            insignificant to JavaScript. Comments will be removed. Tabs will be
            replaced with spaces. Carriage returns will be replaced with linefeeds.
            Most spaces and linefeeds will be removed.
        """
        self.theA = '\n'
        self.theLookahead = EOF
        self.action(3)
        while self.theA != EOF:
            if self.theA == ' ':
                if self.isAlphanum(self.theB):
                    self.action(1)
                else:
                    self.action(2)
            elif self.theA == '\n':
                if self.theB in ['{', '[', '(', '+', '-']:
                    self.action(1)
                elif self.theB == ' ':
                    self.action(3)
                else:
                    if self.isAlphanum(self.theB):
                        self.action(1)
                    else:
                        self.action(2)
            else:
                if self.theB == ' ':
                    if self.isAlphanum(self.theA):
                        self.action(1)
                    else:
                        self.action(3)
                elif self.theB == '\n':
                    if self.theA in ['}', ']', ')', '+', '-', '"', '\'']:
                        self.action(1)
                    else:
                        if self.isAlphanum(self.theA):
                            self.action(1)
                        else:
                            self.action(3)
                else:
                    self.action(1)


if __name__ == '__main__':
    minifier = JavaScriptMinifier()
    minifier.input = sys.stdin
    minifier.output = sys.stdout
    minifier.jsmin()
    sys.stdin.close()
