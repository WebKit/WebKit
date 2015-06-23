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

    builder_name = 'GenericBenchmarkBuilder'

    def prepare(self, name, plan):
        self.name = name
        self.webRoot = tempfile.mkdtemp()
        self.dest = os.path.join(self.webRoot, self.name)
        if 'local_copy' in plan:
            self._copyBenchmarkToTempDir(plan['local_copy'])
        elif 'remote_archive' in plan:
            self._fetchRemoteArchive(plan['remote_archive'])
        elif 'svn_source' in plan:
            self._checkoutWithSubverion(plan['svn_source'])
        else:
            raise Exception('The benchmark location was not specified')

        _log.info('Copied the benchmark into: %s' % self.dest)
        try:
            if 'create_script' in plan:
                self._runCreateScript(plan['create_script'])
            if 'benchmark_patch' in plan:
                self._applyPatch(plan['benchmark_patch'])
            return self.webRoot
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

    def _checkoutWithSubverion(self, subversionURL):
        _log.info('Checking out %s to %s' % (subversionURL, self.dest))
        errorCode = subprocess.call(['svn', 'checkout', subversionURL, self.dest])
        if errorCode:
            raise Exception('Cannot checkout the benchmark - Error: %s' % errorCode)

    def _applyPatch(self, patch):
        oldWorkingDirectory = os.getcwd()
        os.chdir(self.dest)
        errorCode = subprocess.call(['patch', '-p1', '-f', '-i', getPathFromProjectRoot(patch)])
        os.chdir(oldWorkingDirectory)
        if errorCode:
            raise Exception('Cannot apply patch, will skip current benchmarkPath - Error: %s' % errorCode)

    def clean(self):
        _log.info('Cleaning Benchmark')
        if self.webRoot:
            forceRemove(self.webRoot)
