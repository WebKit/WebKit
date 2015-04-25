#!/usr/bin/env python

import abc


class BrowserDriver(object):

    @abc.abstractmethod
    def prepareEnv(self):
        pass

    @abc.abstractmethod
    def launchUrl(self, url, browserBuildPath=None):
        pass

    @abc.abstractmethod
    def closeBrowser(self):
        pass
