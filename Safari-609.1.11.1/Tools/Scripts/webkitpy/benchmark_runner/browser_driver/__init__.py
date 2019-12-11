# Required for Python to search this directory for module files

# Keep this file free of any code or import statements that could
# cause either an error to occur or a log message to be logged.
# This ensures that calling code can import initialization code from
# webkitpy before any errors or log messages due to code in this file.
# Initialization code can include things like version-checking code and
# logging configuration code.
#
# We do not execute any version-checking code or logging configuration
# code in this file so that callers can opt-in as they want.  This also
# allows different callers to choose different initialization code,
# as necessary.
import os

from webkitpy.benchmark_runner.utils import load_subclasses
from browser_driver_factory import BrowserDriverFactory


def browser_driver_loader(browser_driver_class):
    if browser_driver_class.platform and browser_driver_class.browser_name:
        BrowserDriverFactory.add_browser_driver(browser_driver_class.platform, browser_driver_class.browser_name, browser_driver_class)


load_subclasses(
    dirname=os.path.dirname(os.path.abspath(__file__)),
    base_class_name='BrowserDriver',
    base_class_file='browser_driver.py',
    loader=browser_driver_loader)
