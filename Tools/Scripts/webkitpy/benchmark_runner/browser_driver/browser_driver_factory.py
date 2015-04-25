#!/usr/bin/env python

import logging
import json
import os

from webkitpy.benchmark_runner.generic_factory import GenericFactory
from webkitpy.benchmark_runner.utils import loadJSONFromFile


driverFileName = 'browser_drivers.json'


class BrowserDriverFactory(GenericFactory):

    products = loadJSONFromFile(os.path.join(os.path.dirname(__file__), driverFileName))
