# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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


from webkitpy.common.checkout.api import Checkout
from webkitpy.common.checkout.scm import default_scm
from webkitpy.common.config.ports import WebKitPort
from webkitpy.common.net import bugzilla, buildbot, irc, statusserver
from webkitpy.common.system import executive, filesystem, platforminfo, user, workspace
from webkitpy.layout_tests import port


class Host(object):
    def __init__(self):
        self.bugs = bugzilla.Bugzilla()
        self.buildbot = buildbot.BuildBot()
        self.executive = executive.Executive()
        self._irc = None
        self.filesystem = filesystem.FileSystem()
        self.workspace = workspace.Workspace(self.filesystem, self.executive)
        self._port = None
        self.user = user.User()
        self._scm = None
        self._checkout = None
        self.status_server = statusserver.StatusServer()
        self.port_factory = port.factory
        self.platform = platforminfo.PlatformInfo()

    def _initialize_scm(self, patch_directories=None):
        self._scm = default_scm(patch_directories)
        self._checkout = Checkout(self.scm())

    def scm(self):
        return self._scm

    def checkout(self):
        return self._checkout

    def port(self):
        return self._port

    def ensure_irc_connected(self, irc_delegate):
        if not self._irc:
            self._irc = irc.ircproxy.IRCProxy(irc_delegate)

    def irc(self):
        # We don't automatically construct IRCProxy here because constructing
        # IRCProxy actually connects to IRC.  We want clients to explicitly
        # connect to IRC.
        return self._irc

    def command_completed(self):
        if self._irc:
            self._irc.disconnect()
