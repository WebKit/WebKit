#!/usr/bin/python

import os
import sys
import unittest

"""
This is the equivalent of running:
    python -m unittest discover --start-directory {test_discovery_path} --pattern {UNIT_TEST_PATTERN}
"""

UNIT_TEST_PATTERN = '*_unittest.py'


def run_unittests(test_discovery_path):
    test_suite = unittest.defaultTestLoader.discover(test_discovery_path, pattern=UNIT_TEST_PATTERN)
    results = unittest.TextTestRunner(buffer=True).run(test_suite)
    if results.failures or results.errors:
        raise RuntimeError('Test failures or errors were generated during this test run.')


if __name__ == '__main__':
    try:
        relative_path = sys.argv[1]
    except IndexError:
        relative_path = os.path.dirname(__file__)

    path = os.path.abspath(relative_path)
    assert os.path.isdir(path), '{} is not a directory. Please specify a valid directory'.format(path)
    run_unittests(path)
