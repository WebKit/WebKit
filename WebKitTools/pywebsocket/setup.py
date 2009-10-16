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


"""Set up script for mod_pywebsocket.
"""


from distutils.core import setup
import sys


_PACKAGE_NAME = 'mod_pywebsocket'

if sys.version < '2.3':
    print >>sys.stderr, '%s requires Python 2.3 or later.' % _PACKAGE_NAME
    sys.exit(1)

setup(author='Yuzo Fujishima',
      author_email='yuzo@chromium.org',
      description='Web Socket extension for Apache HTTP Server.',
      long_description=(
              'mod_pywebsocket is an Apache HTTP Server extension for '
              'Web Socket (http://tools.ietf.org/html/'
              'draft-hixie-thewebsocketprotocol). '
              'See mod_pywebsocket/__init__.py for more detail.'),
      license='See COPYING',
      name=_PACKAGE_NAME,
      packages=[_PACKAGE_NAME],
      url='http://code.google.com/p/pywebsocket/',
      version='0.4.0',
      )


# vi:sts=4 sw=4 et
