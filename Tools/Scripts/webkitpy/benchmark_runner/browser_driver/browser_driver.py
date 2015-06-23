#!/usr/bin/env python

import abc


class BrowserDriver(object):
    platform = None
    browser_name = None

    @abc.abstractmethod
    def prepareEnv(self, deviceID):
        pass

    @abc.abstractmethod
    def launchUrl(self, url, browserBuildPath=None):
        pass

    @abc.abstractmethod
    def closeBrowser(self):
        pass
