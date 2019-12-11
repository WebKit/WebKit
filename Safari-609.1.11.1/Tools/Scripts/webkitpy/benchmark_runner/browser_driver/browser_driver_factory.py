#!/usr/bin/env python

import logging
import json
import os


class BrowserDriverFactory(object):

    browser_drivers = {}
    platforms = set()
    browsers = set()

    @classmethod
    def available_platforms(cls):
        return list(cls.platforms)

    @classmethod
    def available_browsers(cls):
        return list(cls.browsers)

    @classmethod
    def add_browser_driver(cls, platform, browser_name, browser_driver_class):
        cls.platforms.add(platform)
        cls.browsers.add(browser_name)
        if platform not in cls.browser_drivers:
            cls.browser_drivers[platform] = {}
        cls.browser_drivers[platform][browser_name] = browser_driver_class

    @classmethod
    def create(cls, platform, browser_name):
        if browser_name not in cls.browser_drivers[platform]:
            raise ValueError("Browser \"%s\" is not available on platform \"%s\"" % (browser_name, platform))
        return cls.browser_drivers[platform][browser_name]()
