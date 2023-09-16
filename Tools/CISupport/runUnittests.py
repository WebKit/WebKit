#!/usr/bin/python3

import argparse
import os
import sys
import unittest

"""
This is the equivalent of running:
    python -m unittest discover --start-directory {test_discovery_path} --pattern {UNIT_TEST_PATTERN}
"""

UNIT_TEST_PATTERN = '*_unittest.py'


def run_unittests(test_discovery_path):
    test_suite = unittest.defaultTestLoader.discover(
        test_discovery_path, pattern=UNIT_TEST_PATTERN,
        top_level_dir=os.path.dirname(os.path.abspath(__file__)),
    )
    results = unittest.TextTestRunner(buffer=True).run(test_suite)
    if results.failures or results.errors:
        raise RuntimeError('Test failures or errors were generated during this test run.')
    return 0


def main():
    parser = argparse.ArgumentParser(
        description='Run Python unit tests for our various build masters',
    )
    parser.add_argument(
        'path', nargs=1,
        help='Relative path of directory to run Python unit tests in',
    )
    parser.add_argument(
        '--autoinstall',
        help='Opt in to using autoinstall for buildbot packages',
        action='store_true',
        dest='autoinstall',
        default=False,
    )

    args = parser.parse_args()
    if not args.path:
        args.path = [os.path.dirname(__file__)]

    path = os.path.abspath(args.path[0])
    assert os.path.isdir(path), '{} is not a directory. Please specify a valid directory'.format(path)

    scripts = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Scripts')
    if args.autoinstall and os.path.isdir(os.path.join(scripts, 'webkitpy')):
        sys.path.insert(0, scripts)
        import webkitpy
        import rapidfuzz
        from webkitpy.autoinstalled import buildbot

    # This is work-around for https://bugs.webkit.org/show_bug.cgi?id=222361
    from buildbot.process.buildstep import BuildStep
    BuildStep.warn_deprecated_if_oldstyle_subclass = lambda self, name: None

    return run_unittests(path)


if __name__ == '__main__':
    sys.exit(main())
