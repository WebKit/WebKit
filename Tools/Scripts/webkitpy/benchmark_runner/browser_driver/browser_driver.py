#!/usr/bin/env python

import abc


class BrowserDriver(object):
    platform = None
    browser_name = None

    @abc.abstractmethod
    def prepare_env(self, device_id):
        pass

    @abc.abstractmethod
    def launch_url(self, url, browser_build_path=None):
        pass

    @abc.abstractmethod
    def close_browsers(self):
        pass
