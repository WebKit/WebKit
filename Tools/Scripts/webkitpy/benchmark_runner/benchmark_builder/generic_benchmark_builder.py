#!/usr/bin/env python

import logging
import tempfile
import os
import urllib
import shutil
import subprocess

from zipfile import ZipFile
from webkitpy.benchmark_runner.utils import getPathFromProjectRoot, forceRemove


_log = logging.getLogger(__name__)


class GenericBenchmarkBuilder(object):

    def prepare(self, name, benchmarkPath, archiveURL, patch, createScript):
        self.name = name
        self.webRoot = tempfile.mkdtemp()
        self.dest = os.path.join(self.webRoot, self.name)
        if benchmarkPath:
            self._copyBenchmarkToTempDir(benchmarkPath)
        else:
            assert(archiveURL)
            self._fetchRemoteArchive(archiveURL)

        _log.info('Copied the benchmark into: %s' % self.dest)
        try:
            if createScript:
                self._runCreateScript(createScript)
            return self._applyPatch(patch)
        except Exception:
            self.clean()
            raise

    def _runCreateScript(self, createScript):
        oldWorkingDirectory = os.getcwd()
        os.chdir(self.dest)
        _log.debug('Running %s in %s' % (createScript, self.dest))
        errorCode = subprocess.call(createScript)
        os.chdir(oldWorkingDirectory)
        if errorCode:
            raise Exception('Cannot create the benchmark - Error: %s' % errorCode)

    def _copyBenchmarkToTempDir(self, benchmarkPath):
        shutil.copytree(getPathFromProjectRoot(benchmarkPath), self.dest)

    def _fetchRemoteArchive(self, archiveURL):
        archivePath = os.path.join(self.webRoot, 'archive.zip')
        _log.info('Downloading %s to %s' % (archiveURL, archivePath))
        urllib.urlretrieve(archiveURL, archivePath)

        with ZipFile(archivePath, 'r') as archive:
            archive.extractall(self.dest)

        unarchivedFiles = filter(lambda name: not name.startswith('.'), os.listdir(self.dest))
        if len(unarchivedFiles) == 1:
            firstFile = os.path.join(self.dest, unarchivedFiles[0])
            if os.path.isdir(firstFile):
                shutil.move(firstFile, self.webRoot)
                os.rename(os.path.join(self.webRoot, unarchivedFiles[0]), self.dest)

    def _applyPatch(self, patch):
        if not patch:
            return self.webRoot
        oldWorkingDirectory = os.getcwd()
        os.chdir(self.dest)
        errorCode = subprocess.call(['patch', '-p1', '-f', '-i', getPathFromProjectRoot(patch)])
        os.chdir(oldWorkingDirectory)
        if errorCode:
            raise Exception('Cannot apply patch, will skip current benchmarkPath - Error: %s' % errorCode)
        return self.webRoot

    def clean(self):
        _log.info('Cleaning Benchmark')
        if self.webRoot:
            forceRemove(self.webRoot)
