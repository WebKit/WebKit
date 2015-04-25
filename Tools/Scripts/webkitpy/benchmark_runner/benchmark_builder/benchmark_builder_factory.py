#!/usr/bin/env python

import logging
import json
import os

from webkitpy.benchmark_runner.generic_factory import GenericFactory
from webkitpy.benchmark_runner.utils import loadJSONFromFile


builderFileName = 'benchmark_builders.json'


class BenchmarkBuilderFactory(GenericFactory):

    products = loadJSONFromFile(os.path.join(os.path.dirname(__file__), builderFileName))
