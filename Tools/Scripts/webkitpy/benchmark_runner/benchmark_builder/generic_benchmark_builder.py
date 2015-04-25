#!/usr/bin/env python

import logging
import tempfile
import os
import shutil
import subprocess

from webkitpy.benchmark_runner.utils import getPathFromProjectRoot


_log = logging.getLogger(__name__)


class GenericBenchmarkBuilder(object):

    def prepare(self, benchmarkPath, patch):
        self._copyBenchmarkToTempDir(benchmarkPath)
        return self._applyPatch(patch)

    def _copyBenchmarkToTempDir(self, benchmarkPath):
        self.webRoot = tempfile.mkdtemp()
        _log.debug('Servering at webRoot: %s' % self.webRoot)
        self.dest = os.path.join(self.webRoot, os.path.split(benchmarkPath)[1])
        shutil.copytree(getPathFromProjectRoot(benchmarkPath), self.dest)

    def _applyPatch(self, patch):
        if not patch:
            return self.webRoot
        oldWorkingDirectory = os.getcwd()
        os.chdir(self.webRoot)
        errorCode = subprocess.call(['patch', '-p1', '-f', '-i', getPathFromProjectRoot(patch)])
        os.chdir(oldWorkingDirectory)
        if errorCode:
            _log.error('Cannot apply patch, will skip current benchmarkPath')
            self.clean()
            return None
        return self.webRoot

    def clean(self):
        _log.info('Cleanning Benchmark')
        if self.webRoot:
            shutil.rmtree(self.webRoot)
