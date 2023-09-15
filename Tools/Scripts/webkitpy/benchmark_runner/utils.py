import inspect
import logging
import os
import shutil

from webkitpy.common.memoized import memoized


try:
    import importlib.util
    import importlib.machinery

    def load_source(modname, filename):
        loader = importlib.machinery.SourceFileLoader(modname, filename)
        spec = importlib.util.spec_from_file_location(modname, filename, loader=loader)
        module = importlib.util.module_from_spec(spec)
        loader.exec_module(module)
        return module
except ImportError:
    from imp import load_source


_log = logging.getLogger(__name__)


# Borrow following code from stackoverflow
# Link: http://stackoverflow.com/questions/11461356/issubclass-returns-flase-on-the-same-class-imported-from-different-paths
def is_subclass(child, parent_name):
    return inspect.isclass(child) and parent_name in [cls.__name__ for cls in inspect.getmro(child)]


def load_subclasses(dirname, base_class_name, base_class_file, loader):
    filelist = [base_class_file] + [f for f in os.listdir(dirname) if f.endswith('_' + base_class_file)]
    filelist += [f for f in os.listdir(dirname) if f.endswith('.py') and f not in ['__init__.py'] + filelist]
    for filename in filelist:
        module_name = os.path.splitext(filename)[0]
        module = load_source(module_name, os.path.join(dirname, filename))
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
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
    except Exception as error:
        # Directory/file does not exist or privilege issue, just ignore it
        _log.info("Error removing %s: %s" % (path, error))
        pass


@memoized
def get_driver_binary_path(browser_name):
    if browser_name.startswith('chrome'):
        from webkitpy.autoinstalled import chromedriver
        return chromedriver.executable
    elif browser_name.startswith('firefox'):
        from webkitpy.autoinstalled import geckodriver
        return geckodriver.executable


def write_defaults(domain, key, value):
    # Returns whether the key in the domain is updated
    from Foundation import NSUserDefaults
    defaults = NSUserDefaults.standardUserDefaults()
    defaults_for_domain = defaults.persistentDomainForName_(domain)
    if not defaults_for_domain:
        return False
    old_value = defaults_for_domain.get(key)
    if old_value == value:
        return False
    mutable_defaults_for_domain = defaults_for_domain.mutableCopy()
    mutable_defaults_for_domain[key] = value
    defaults.setPersistentDomain_forName_(mutable_defaults_for_domain, domain)
    defaults.synchronize()
    return True
