import glob
import os.path
import sys

top_level_dir = None


def top_level_path(*args):
    global top_level_dir
    if not top_level_dir:
        top_level_dir = os.path.join(os.path.dirname(__file__), '..', '..')
    return os.path.join(*(top_level_dir,) + args)


def get_dependencies_path():
    if 'WEBKITOUTPUTDIR' in os.environ:
        return os.path.abspath(os.path.join(os.environ['WEBKITOUTPUTDIR'], 'Dependencies'))
    else:
        return os.path.abspath(top_level_path('WebKitBuild', 'Dependencies'))


def get_config_file_for_platform(platform):
    return top_level_path('Tools', platform, 'jhbuildrc')


def enter_jhbuild_environment_if_available(platform):
    if not os.path.exists(get_dependencies_path()):
        return False

    # We don't know the Python version, so we just assume that we can safely take the first one in the list.
    site_packages_path = glob.glob(os.path.join(get_dependencies_path(), "Root", "lib", "*", "site-packages"))
    if not len(site_packages_path):
        return False

    site_packages_path = site_packages_path[0]
    sys.path.insert(0, site_packages_path)

    import jhbuild.config
    from jhbuild.errors import FatalError

    try:
        config = jhbuild.config.Config(get_config_file_for_platform(platform))
    except FatalError, exception:
        sys.stderr.write('Could not load jhbuild config file: %s\n' % exception.args[0])
        return False

    return True
