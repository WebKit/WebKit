# Copyright (C) 2017 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
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


class WebDriver(object):
    def __init__(self, port):
        self._port = port

    def binary_path(self):
        raise NotImplementedError

    def browser_name(self):
        raise NotImplementedError

    def capabilities(self):
        raise NotImplementedError

    def browser_path(self):
        return None

    def browser_args(self):
        return []

    def browser_env(self):
        return {}

    def selenium_name(self):
        """Return the name of the driver used by Selenium, passed to pytest using --driver command line option.
          If this is not implemented, Selenium tests will not be run."""
        return None

_drivers = {}


def register_driver(port_name, driver_cls):
    _drivers[port_name] = driver_cls


def create_driver(port):
    if port.name() not in _drivers:
        __import__('webkitpy.webdriver_tests.webdriver_driver_%s' % port.name())

    return _drivers[port.name()](port)
