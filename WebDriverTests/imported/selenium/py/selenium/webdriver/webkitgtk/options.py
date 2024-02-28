# Licensed to the Software Freedom Conservancy (SFC) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The SFC licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

from selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from selenium.webdriver.common.options import ArgOptions


class Options(ArgOptions):
    KEY = 'webkitgtk:browserOptions'

    def __init__(self):
        super(Options, self).__init__()
        self._binary_location = ''
        self._overlay_scrollbars_enabled = True
        self._target_ip = None
        self._target_port = None

    @property
    def binary_location(self):
        """
        :Returns: The location of the browser binary otherwise an empty string
        """
        return self._binary_location

    @binary_location.setter
    def binary_location(self, value):
        """
        Allows you to set the browser binary to launch

        :Args:
         - value : path to the browser binary
        """
        self._binary_location = value

    @property
    def overlay_scrollbars_enabled(self):
        """
        :Returns: Whether overlay scrollbars should be enabled
        """
        return self._overlay_scrollbars_enabled

    @overlay_scrollbars_enabled.setter
    def overlay_scrollbars_enabled(self, value):
        """
        Allows you to enable or disable overlay scrollbars

        :Args:
         - value : True or False
        """
        self._overlay_scrollbars_enabled = value

    @property
    def page_load_strategy(self):
        return self._caps["pageLoadStrategy"]

    @page_load_strategy.setter
    def page_load_strategy(self, strategy):
        if strategy in ["normal", "eager", "none"]:
            self.set_capability("pageLoadStrategy", strategy)
        else:
            raise ValueError("Strategy can only be one of the following: normal, eager, none")

    @property
    def target_ip(self):
        """
        :Returns: The IP address of existing browser instance running in automation mode othewise None
        """
        return self._target_ip

    @target_ip.setter
    def target_ip(self, value):
        """
        Allows you to set the tcp port of existing browser instance running in automation mode

        :Args:
         - value : ip address
        """
        self._target_ip = value

    @property
    def target_port(self):
        """
        :Returns: The IP port number of existing browser instance running in automation mode othewise None
        """
        return self._target_port

    @target_port.setter
    def target_port(self, value):
        """
        Allows you to set the IP port number of existing browser instance running in automation mode

        :Args:
         - value : port number
        """
        self._target_port = value

    def to_capabilities(self):
        """
        Creates a capabilities with all the options that have been set and
        returns a dictionary with everything
        """
        caps = self._caps

        browser_options = {}
        if self.binary_location:
            browser_options["binary"] = self.binary_location
        if self.arguments:
            browser_options["args"] = self.arguments
        browser_options["useOverlayScrollbars"] = self.overlay_scrollbars_enabled
        if self.target_ip:
            browser_options["targetAddr"] = self.target_ip
        if self.target_port:
            browser_options["targetPort"] = int(self.target_port)

        caps[Options.KEY] = browser_options

        return caps

    @property
    def default_capabilities(self):
        return DesiredCapabilities.WEBKITGTK.copy()
