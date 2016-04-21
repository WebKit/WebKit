#!/usr/bin/env python

from abc import ABCMeta, abstractmethod

class BrowserDriver(object):
    platform = None
    browser_name = None

    ___metaclass___ = ABCMeta

    @abstractmethod
    def prepare_env(self, device_id):
        pass

    @abstractmethod
    def launch_url(self, url, options, browser_build_path=None):
        pass

    @abstractmethod
    def add_additional_results(self, test_url, results):
        return results

    @abstractmethod
    def close_browsers(self):
        pass

    @abstractmethod
    def restore_env(self):
        pass
