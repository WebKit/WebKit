import logging

from abc import ABCMeta, abstractmethod
from webkitpy.benchmark_runner.utils import get_driver_binary_path
from contextlib import contextmanager

_log = logging.getLogger(__name__)


class BrowserDriver(object):
    platform = None
    browser_name = None

    ___metaclass___ = ABCMeta

    def __init__(self, browser_args):
        self.browser_args = browser_args

    @abstractmethod
    def prepare_env(self, config):
        pass

    @abstractmethod
    def prepare_initial_env(self, config):
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

    @abstractmethod
    def restore_env_after_all_testing(self):
        pass

    def diagnose_test_failure(self, debug_directory, error):
        pass

    @contextmanager
    def prevent_sleep(self, timeout):
        yield

    @contextmanager
    def profile(self, output_path, profile_filename, profiling_interval, trace_type='profile', timeout=300):
        _log.error('The --profile option was specified, but an empty context was called. This run will not be profiled.')
        yield

    @property
    def webdriver_binary_path(self):
        return get_driver_binary_path(self.browser_name)
