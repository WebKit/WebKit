#!/usr/bin/env python

import json
import logging
import shutil
import signal
import subprocess
import tempfile
import time
import types
import os
import urlparse

from benchmark_builder.benchmark_builder_factory import BenchmarkBuilderFactory
from browser_driver.browser_driver_factory import BrowserDriverFactory
from http_server_driver.http_server_driver_factory import HTTPServerDriverFactory
from utils import loadModule, getPathFromProjectRoot
from utils import timeout


_log = logging.getLogger(__name__)


class BenchmarkRunner(object):

    def __init__(self, planFile, buildDir, outputFile, platform, browser):
        _log.info('Initializing benchmark running')
        try:
            with open(planFile, 'r') as fp:
                self.plan = json.load(fp)
                self.browserDriver = BrowserDriverFactory.create([platform, browser])
                self.httpServerDriver = HTTPServerDriverFactory.create([self.plan['http_server_driver']])
                self.buildDir = os.path.abspath(buildDir)
                self.outputFile = outputFile
        except IOError:
            _log.error('Can not open plan file: %s' % planFile)
        except ValueError:
            _log.error('Plan file:%s may not follow JSON format' % planFile)
        except:
            raise

    def execute(self):
        _log.info('Start to execute the plan')
        _log.info('Start a new benchmark')
        results = []
        benchmarkBuilder = BenchmarkBuilderFactory.create([self.plan['benchmark_builder']])
        webRoot = benchmarkBuilder.prepare(self.plan['original_benchmark'], self.plan['benchmark_patch'] if 'benchmark_patch' in self.plan else None)
        for x in xrange(int(self.plan['count'])):
            _log.info('Start the iteration %d of current benchmark' % (x + 1))
            self.httpServerDriver.serve(webRoot)
            self.browserDriver.prepareEnv()
            self.browserDriver.launchUrl(urlparse.urljoin(self.httpServerDriver.baseUrl(), self.plan['entry_point']), self.buildDir)
            try:
                with timeout(self.plan['timeout']):
                    result = json.loads(self.httpServerDriver.fetchResult())
                assert(not self.httpServerDriver.getReturnCode())
                assert(result)
                results.append(result)
            except:
                _log.error('No result or server crashes. Something went wrong. Will skip current benchmark.')
                self.browserDriver.closeBrowsers()
                self.httpServerDriver.killServer()
                benchmarkBuilder.clean()
                return 1
            finally:
                self.browserDriver.closeBrowsers()
                _log.info('End of %d iteration of current benchmark' % (x + 1))
        results = self.wrap(results)
        self.dump(results, self.outputFile if self.outputFile else self.plan['output_file'])
        benchmarkBuilder.clean()
        return 0

    @classmethod
    def dump(cls, results, outputFile):
        _log.info('Dumping the results to file')
        try:
            with open(outputFile, 'w') as fp:
                json.dump(results, fp)
        except IOError:
            _log.error('Cannot open output file: %s' % outputFile)
            _log.error('Results are:\n %s', json.dumps(results))

    @classmethod
    def wrap(cls, dicts):
        _log.info('Merging following results:\n%s', json.dumps(dicts))
        if not dicts:
            return None
        ret = {}
        for dic in dicts:
            ret = cls.merge(ret, dic)
        _log.info('Results after merging:\n%s', json.dumps(ret))
        return ret

    @classmethod
    def merge(cls, a, b):
        assert(isinstance(a, type(b)))
        argType = type(a)
        # special handle for list type, and should be handle before equal check
        if argType == types.ListType and len(a) and (type(a[0]) == types.StringType or type(a[0]) == types.UnicodeType):
            return a
        if argType == types.DictType:
            result = {}
            for key, value in a.items():
                if key in b:
                    result[key] = cls.merge(value, b[key])
                else:
                    result[key] = value
            for key, value in b.items():
                if key not in result:
                    result[key] = value
            return result
        # for other types
        return a + b
