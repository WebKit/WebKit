#!/usr/bin/env python

from abc import abstractmethod, ABCMeta


class HTTPServerDriver(object):
    platforms = []

    __metaclass__ = ABCMeta

    @abstractmethod
    def serve(self, webRoot):
        pass

    @abstractmethod
    def fetch_result(self):
        pass

    @abstractmethod
    def kill_server(self):
        pass

    @abstractmethod
    def get_return_code(self):
        pass

    @abstractmethod
    def set_device_id(self, device_id):
        pass
