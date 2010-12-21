#!/usr/bin/env python
import glob, trace, unittest

# find all of the planet test modules
modules = map(trace.fullmodname, glob.glob('planet/tests/test_*.py'))

# load all of the tests into a suite
suite = unittest.TestLoader().loadTestsFromNames(modules)

# run test suite
unittest.TextTestRunner().run(suite)
