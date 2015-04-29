#!/usr/bin/env python

import json
import logging
import os
import signal
import shutil


_log = logging.getLogger(__name__)


class ModuleNotFoundError(Exception):
    pass


def loadModule(moduleDesc):
    try:
        ret = getattr(__import__(moduleDesc['filePath'], globals(), locals(), moduleDesc['moduleName'], -1), moduleDesc['moduleName'])
        return ret
    except Exception as e:
        raise ModuleNotFoundError('Module (%s) with path(%s) is not found' % (moduleDesc['moduleName'], moduleDesc['filePath']))


def getPathFromProjectRoot(relativePathToProjectRoot):
    # Choose the directory containning current file as start point,
    # compute relative path base on the parameter,
    # and return an absolute path
    return os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), relativePathToProjectRoot))


def loadJSONFromFile(filePath):
    try:
        jsonObject = json.load(open(filePath, 'r'))
        assert(jsonObject)
        return jsonObject
    except:
        raise Exception("Invalid json format or empty json was found in %s" % (filePath))


def forceRemove(path):
    try:
        shutil.rmtree(path)
    except:
        # Directory/file does not exist or privilege issue, just ignore it
        pass

# Borrow this code from
# 'http://stackoverflow.com/questions/2281850/timeout-function-if-it-takes-too-long-to-finish'
class TimeoutError(Exception):
    pass


class timeout:

    def __init__(self, seconds=1, error_message='Timeout'):
        self.seconds = seconds
        self.error_message = error_message

    def handle_timeout(self, signum, frame):
        raise TimeoutError(self.error_message)

    def __enter__(self):
        signal.signal(signal.SIGALRM, self.handle_timeout)
        signal.alarm(self.seconds)

    def __exit__(self, type, value, traceback):
        signal.alarm(0)
