#!/usr/bin/env python

from abc import abstractmethod


class HTTPServerDriver(object):
    name = None
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
    def set_device_id(self, deviceID):
        pass
