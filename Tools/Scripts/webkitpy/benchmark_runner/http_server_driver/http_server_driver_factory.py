#!/usr/bin/env python

import logging
import json
import os

from webkitpy.benchmark_runner.generic_factory import GenericFactory
from webkitpy.benchmark_runner.utils import loadJSONFromFile


driverFileName = 'http_server_drivers.json'


class HTTPServerDriverFactory(GenericFactory):

    products = loadJSONFromFile(os.path.join(os.path.dirname(__file__), driverFileName))
