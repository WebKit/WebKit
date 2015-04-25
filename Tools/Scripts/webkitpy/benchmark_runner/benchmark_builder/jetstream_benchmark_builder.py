#!/usr/bin/env python

import logging
import os
import subprocess

from generic_benchmark_builder import GenericBenchmarkBuilder


_log = logging.getLogger(__name__)


class JetStreamBenchmarkBuilder(GenericBenchmarkBuilder):

    def prepare(self, benchmarkPath, patch):
        super(self.__class__, self)._copyBenchmarkToTempDir(benchmarkPath)
        self._runCreateScript()
        return super(self.__class__, self)._applyPatch(patch)

    def _runCreateScript(self):
        oldWorkingDirectory = os.getcwd()
        os.chdir(self.dest)
        _log.debug(self.dest)
        errorCode = subprocess.call(['ruby', 'create.rb'])
        os.chdir(oldWorkingDirectory)
        if errorCode:
            _log.error('Cannot create JetStream Benchmark')
