#!/usr/bin/env python

from abc import abstractmethod


class HTTPServerDriver(object):
    name = None
    @abstractmethod
    def serve(self, webRoot):
        pass

    @abstractmethod
    def fetchResult(self):
        pass

    @abstractmethod
    def killServer(self):
        pass

    @abstractmethod
    def getReturnCode(self):
        pass

    @abstractmethod
    def setDeviceID(self, deviceID):
        pass
