#!/usr/bin/env python

import logging
import json
import os

from webkitpy.benchmark_runner.generic_factory import GenericFactory
from webkitpy.benchmark_runner.utils import loadJSONFromFile


driverFileName = 'browser_drivers.json'


class BrowserDriverFactory(GenericFactory):

    products = loadJSONFromFile(os.path.join(os.path.dirname(__file__), driverFileName))

    @classmethod
    def available_platforms(cls):
        return cls.products.keys()

    @classmethod
    def available_browsers(cls):
        browsers = []
        for platform in cls.products.values():
            for browser in platform:
                browsers.append(browser)
        return browsers
