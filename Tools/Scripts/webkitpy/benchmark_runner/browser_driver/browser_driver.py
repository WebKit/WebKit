#!/usr/bin/env python

from abc import ABCMeta, abstractmethod
from webkitpy.benchmark_runner.utils import get_driver_binary_path


class BrowserDriver(object):
    platform = None
    browser_name = None

    ___metaclass___ = ABCMeta

    @abstractmethod
    def prepare_env(self, config):
        pass

    @abstractmethod
    def launch_url(self, url, options, browser_build_path=None, browser_path=None):
        pass

    @abstractmethod
    def launch_webdriver(self, url, driver):
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

    @property
    def webdriver_binary_path(self):
        return get_driver_binary_path(self.browser_name)
