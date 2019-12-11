#!/usr/bin/env python

import logging
import json
import os

from webkitpy.benchmark_runner.generic_factory import GenericFactory


class HTTPServerDriverFactory(GenericFactory):
    products = {}
