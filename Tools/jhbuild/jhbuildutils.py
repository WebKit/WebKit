import gettext
import glob
import os.path
import sys
try:
    import __builtin__
except ImportError:
    import builtins
    __builtin__ = builtins

top_level_dir = None


def top_level_path(*args):
    global top_level_dir
    if not top_level_dir:
        top_level_dir = os.path.join(os.path.dirname(__file__), '..', '..')
    return os.path.join(*(top_level_dir,) + args)


def get_dependencies_path(platform):
    dependencies_dir = "%s%s" % ('Dependencies', platform.upper())
    if 'WEBKIT_OUTPUTDIR' in os.environ:
        return os.path.abspath(os.path.join(os.environ['WEBKIT_OUTPUTDIR'], dependencies_dir))
    else:
        return os.path.abspath(top_level_path('WebKitBuild', dependencies_dir))


def get_config_file_for_platform(platform):
    return top_level_path('Tools', platform, 'jhbuildrc')


def enter_jhbuild_environment_if_available(platform):
    if not os.path.exists(get_dependencies_path(platform)):
        return False

    # Sometimes jhbuild chooses to install in a way that reads the library from the source directory, so fall
    # back to that method.
    source_path = os.path.join(get_dependencies_path(platform), "Source", "jhbuild")
    sys.path.insert(0, source_path)

    # When loading jhbuild from the source checkout it fails if the SRCDIR, PKGDATADIR or DATADIR aren't present.
    __builtin__.__dict__['SRCDIR'] = source_path
    __builtin__.__dict__['PKGDATADIR'] = None
    __builtin__.__dict__['DATADIR'] = None

    # We don't know the Python version, so we just assume that we can safely take the first one in the list.
    site_packages_path = glob.glob(os.path.join(get_dependencies_path(platform), "Root", "lib", "*", "site-packages"))
    if len(site_packages_path):
       site_packages_path = site_packages_path[0]
       sys.path.insert(0, site_packages_path)

    try:
        import jhbuild.config
        from jhbuild.errors import FatalError
        gettext.install('jhbuild', localedir=os.path.join(source_path, 'mo'))
        config = jhbuild.config.Config(get_config_file_for_platform(platform), [])
    except FatalError as exception:
        sys.stderr.write('Could not load jhbuild config file: %s\n' % exception.args[0])
        return False

    return True
