#!/usr/bin/env python

import imp
import inspect
import logging
import os
import signal
import shutil
import sys


_log = logging.getLogger(__name__)


# Borrow following code from stackoverflow
# Link: http://stackoverflow.com/questions/11461356/issubclass-returns-flase-on-the-same-class-imported-from-different-paths
def is_subclass(child, parent_name):
    return inspect.isclass(child) and parent_name in [cls.__name__ for cls in inspect.getmro(child)]


def load_subclasses(dirname, base_class_name, base_class_file, loader):
    filelist = [base_class_file] + [f for f in os.listdir(dirname) if f.endswith('.py') and f not in ['__init__.py', base_class_file]]
    for filename in filelist:
        module_name = os.path.splitext(filename)[0]
        module = imp.load_source(module_name, os.path.join(dirname, filename))
        for item_name in dir(module):
            item = getattr(module, item_name)
            if is_subclass(item, base_class_name):
                loader(item)


def get_path_from_project_root(relative_path_to_project_root):
    # Choose the directory containing current file as start point,
    # compute relative path base on the parameter,
    # and return an absolute path
    return os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), relative_path_to_project_root))


def force_remove(path):
    try:
        shutil.rmtree(path)
    except Exception as error:
        # Directory/file does not exist or privilege issue, just ignore it
        _log.info("Error removing %s: %s" % (path, error))
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
