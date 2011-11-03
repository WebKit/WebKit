# Copyright (C) 2011 Google Inc. All rights reserved.
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

import threading

from webkitpy.common.net.bugzilla.bugzilla_mock import MockBugzilla
from webkitpy.common.net.statusserver_mock import MockStatusServer
from webkitpy.common.net.buildbot.buildbot_mock import MockBuildBot
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.user_mock import MockUser
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.platforminfo_mock import MockPlatformInfo
from webkitpy.common.system.workspace_mock import MockWorkspace
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.net.irc.irc_mock import MockIRC
from webkitpy.common.checkout.checkout_mock import MockCheckout
from webkitpy.common.checkout.scm.scm_mock import MockSCM
from webkitpy.common.watchlist.watchlist_mock import MockWatchList

# FIXME: Old-style "Ports" need to die and be replaced by modern layout_tests.port which needs to move to common.
from webkitpy.common.config.ports_mock import MockPort
from webkitpy.layout_tests.port.factory import PortFactory


class MockHost(object):
    def __init__(self, log_executive=False, executive_throws_when_run=None):
        self.executive = MockExecutive(should_log=log_executive, should_throw_when_run=executive_throws_when_run)
        self.user = MockUser()
        self.platform = MockPlatformInfo()
        self.workspace = MockWorkspace()
        self.web = MockWeb()

        self._checkout = MockCheckout()
        self._scm = MockSCM()
        # Various pieces of code (wrongly) call filesystem.chdir(checkout_root).
        # Making the checkout_root exist in the mock filesystem makes that chdir not raise.
        self.filesystem = MockFileSystem(dirs=set([self._scm.checkout_root]))

        self.bugs = MockBugzilla()
        self.buildbot = MockBuildBot()
        self._chromium_buildbot = MockBuildBot()
        self.status_server = MockStatusServer()

        self._irc = None
        self.irc_password = "MOCK irc password"
        self.wakeup_event = threading.Event()

        self._port = MockPort()
        # Note: We're using a real PortFactory here.  Tests which don't wish to depend
        # on the list of known ports should override this with a MockPortFactory.
        self.port_factory = PortFactory(self)

        self._watch_list = MockWatchList()

    def scm(self):
        return self._scm

    def checkout(self):
        return self._checkout

    def port(self):
        return self._port

    def chromium_buildbot(self):
        return self._chromium_buildbot

    def watch_list(self):
        return self._watch_list

    def ensure_irc_connected(self, delegate):
        if not self._irc:
            self._irc = MockIRC()

    def irc(self):
        return self._irc
