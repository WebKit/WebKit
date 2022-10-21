# -*- coding: utf-8 -*-
# Copyright (C) 2017 Igalia S.L.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
# Boston, MA 02110-1301, USA.
import argparse
import atexit
import logging
try:
    import configparser
except ImportError:
    import ConfigParser as configparser
from contextlib import contextmanager
import errno
import json
import multiprocessing
import os
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import re
import platform

SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
WEBKIT_SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..")))
sys.path.insert(0, os.path.join(WEBKIT_SOURCE_DIR, "Tools", "Scripts"))
from webkitpy.common.system.logutils import configure_logging
from webkitcorepy import string_utils
import toml
import json

try:
    from urllib.parse import urlparse  # pylint: disable=E0611
except ImportError:
    from urlparse import urlparse

try:
    from urllib.request import urlopen  # pylint: disable=E0611
except ImportError:
    from urllib2 import urlopen

try:
    from contextlib import nullcontext
except ImportError:
    @contextmanager
    def nullcontext(enter_result=None):
        yield enter_result

FLATPAK_REQUIRED_VERSION = "1.4.4"

_log = logging.getLogger(__name__)

BUILD_ROOT_DIR_NAME = 'WebKitBuild'

# This path doesn't take $WEBKIT_OUTPUTDIR in account because the standalone toolchains
# paths depend on it and those are also hard-coded in the generated sccache config.
DEFAULT_BUILD_ROOT = os.path.join(WEBKIT_SOURCE_DIR, BUILD_ROOT_DIR_NAME)
BUILD_ROOT = os.path.join(os.environ.get("WEBKIT_OUTPUTDIR", WEBKIT_SOURCE_DIR), BUILD_ROOT_DIR_NAME)
FLATPAK_USER_DIR_PATH = os.path.realpath(os.path.join(DEFAULT_BUILD_ROOT, "UserFlatpak"))

DEFAULT_SCCACHE_SCHEDULER='https://sccache.igalia.com'

# Where the source folder is mounted inside the sandbox.
SANDBOX_SOURCE_ROOT = "/app/webkit"

# Our SDK branch matches with the FDO SDK branch. When updating the FDO SDK release branch
# in our SDK build definitions please don't forget to update the version here as well.
SDK_BRANCH = "22.08"

WEBKIT_SDK_FLATPAK_REPO_URL = "https://software.igalia.com/flatpak-refs/webkit-sdk.flatpakrepo"
WEBKIT_SDK_GPG_PUBKEY_URL = "https://software.igalia.com/flatpak-refs/webkit-sdk-pubkey.gpg"
WEBKIT_SDK_REPO_URL = "https://software.igalia.com/webkit-sdk-repo/"

is_colored_output_supported = False
try:
    import curses
    assert sys.stdout.isatty()
    curses.setupterm()
    assert curses.tigetnum("colors") > 0
except Exception:
    is_colored_output_supported = False
else:
    is_colored_output_supported = True

class Colors:
    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKGREEN = "\033[92m"
    WARNING = "\033[93m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"


class Console:

    quiet = False

    @classmethod
    def message(cls, str_format, *args):
        if cls.quiet:
            return

        if args:
            print(str_format % args)
        else:
            print(str_format)

        # Flush so that messages are printed at the right time
        # as we use many subprocesses.
        sys.stdout.flush()

    @classmethod
    def colored_message_if_supported(cls, color, str_format, *args):
        if args:
            msg = str_format % args
        else:
            msg = str_format

        if is_colored_output_supported:
            cls.message("\n%s%s%s", color, msg, Colors.ENDC)
        else:
            cls.message(msg)

    @classmethod
    def error_message(cls, str_format, *args):
        cls.colored_message_if_supported(Colors.FAIL, str_format, *args)

    @classmethod
    def warning_message(cls, str_format, *args):
        cls.colored_message_if_supported(Colors.WARNING, str_format, *args)


def run_sanitized(command, gather_output=False, ignore_stderr=False, env=None, **kwargs):
    """ Runs a command in a santized environment and optionally returns decoded output or raises
        subprocess.CalledProcessError
    """
    if env:
        sanitized_env = env.copy()
    else:
        sanitized_env = os.environ.copy()

    # We need clean output free of debug messages
    try:
        del sanitized_env["G_MESSAGES_DEBUG"]
    except KeyError:
        pass

    _log.debug("Running %s", " ".join(command))
    keywords = kwargs
    keywords['env'] = sanitized_env
    if gather_output:
        if ignore_stderr:
            with open(os.devnull, 'w') as devnull:
                output = subprocess.check_output(command, stderr=devnull, **keywords)
        else:
            output = subprocess.check_output(command, **keywords)
        return output.strip().decode('utf-8')
    else:
        keywords["stdout"] = sys.stdout
        if ignore_stderr:
            with open(os.devnull, 'w') as devnull:
                keywords["stderr"] = devnull
                return subprocess.check_call(command, **keywords)
        else:
            return subprocess.check_call(command, **keywords)


def check_flatpak(verbose=True):
    # Flatpak is only supported on Linux.
    if not sys.platform.startswith("linux"):
        return ()

    required_version = FLATPAK_REQUIRED_VERSION
    try:
        output = run_sanitized(["flatpak", "--version"], gather_output=True)
    except (subprocess.CalledProcessError, OSError):
        if verbose:
            Console.error_message("You need to install flatpak >= %s"
                                  " to be able to use the '%s' script.\n\n"
                                  "You can find some informations about"
                                  " how to install it for your distribution at:\n"
                                  "    * https://flatpak.org/\n", required_version,
                                  sys.argv[0])
            return ()

    def comparable_version(version):
        return tuple(map(int, (version.split("."))))

    version = output.split(" ")[1].strip("\n")
    current_version = comparable_version(version)
    if current_version < comparable_version(required_version):
        Console.error_message("flatpak %s required but %s found. Please update and try again\n",
                              required_version, version)
        return ()

    return current_version


def convert_webkit_source_path_to_sandbox_path(source_path):
    '''Convert a path in the WebKit source directory to the same path in the
       sandboxed source diretory. '''
    return source_path.replace(WEBKIT_SOURCE_DIR, SANDBOX_SOURCE_ROOT)


def convert_sandbox_path_to_webkit_source_path(sandbox_path):
    # For now this supports only files in the /app/webkit path
    return sandbox_path.replace(SANDBOX_SOURCE_ROOT, WEBKIT_SOURCE_DIR)


def get_build_dir(platform, build_type):
    return os.path.join(BUILD_ROOT, platform, build_type)

class FlatpakObject:

    def __init__(self, user):
        self.user = user

    def flatpak(self, command, *args, **kwargs):
        comment = kwargs.pop("comment", None)
        gather_output = kwargs.get("gather_output", False)
        if comment:
            Console.message(comment)

        command = ["flatpak", command]
        help_output = run_sanitized(command + ["--help"], gather_output=True)
        if self.user and "--user" in help_output:
            command.append("--user")
        if "--assumeyes" in help_output:
            command.append("--assumeyes")
        if "--noninteractive" in help_output and gather_output:
            command.append("--noninteractive")

        command.extend(args)

        return run_sanitized(command, **kwargs)

    def version(self, ref_id):
        try:
            output = self.flatpak("info", ref_id, gather_output=True, ignore_stderr=True)
        except subprocess.CalledProcessError:
            # ref is likely not installed
            return ""
        for line in output.splitlines():
            tokens = line.split(":")
            if len(tokens) != 2:
                continue
            if tokens[0].strip().lower() == "version":
                return tokens[1].strip()
        return ""

    def flatpak_update(self):
        remote = "webkit-sdk"
        try:
            self.flatpak("remote-ls", remote, gather_output=True)
        except subprocess.CalledProcessError as error:
            if error.output.lower().find(b"key expired"):
                Console.message("WebKit SDK GPG key expired, synchronizing with remote")
                with tempfile.NamedTemporaryFile() as tmpfile:
                    fd = urlopen(WEBKIT_SDK_GPG_PUBKEY_URL)
                    tmpfile.write(fd.read())
                    tmpfile.flush()
                    self.flatpak("remote-modify", "--gpg-import=" + tmpfile.name, remote)

        self.flatpak("repair", comment="Ensuring the local Flatpak repository is not corrupted")
        self.flatpak("update", comment="Updating Flatpak environment")

class FlatpakPackages(FlatpakObject):

    def __init__(self, repos, user=True):
        FlatpakObject.__init__(self, user=user)
        self.repos = repos
        self.update()

    def update(self):
        self.packages = []
        out = self.flatpak("list", "--columns=application,arch,branch,origin", "-a", gather_output=True)
        package_defs = [line for line in out.split("\n") if line]
        for package_def in package_defs:
            name, arch, branch, origin = package_def.split("\t")

            # If installed from a file, the package is in no repo
            repo = self.repos.repos.get(origin)
            self.packages.append(FlatpakPackage(name, branch, repo, arch))

    def __iter__(self):
        for package in self.packages:
            yield package


class FlatpakRepos(FlatpakObject):

    def __init__(self, repos, user=True):
        FlatpakObject.__init__(self, user=user)
        self.update()

        updated = False
        for repo in repos:
            updated = self.add(repo, update=False) or updated

        # Only fetch the remote and package list again if we updated anything.
        if updated:
            self.update()

    def update(self):
        self.repos = {}
        out = self.flatpak("remote-list", "--columns=name,title,url", gather_output=True)
        remotes = [line for line in out.split("\n") if line]
        for remote in remotes:
            name, title, url = remote.split("\t")
            parsed_url = urlparse(url)
            if not parsed_url.scheme:
                Console.message("No valid URI found for: %s", remote)
                continue

            self.repos[name] = FlatpakRepo(name=name, url=url, desc=title, repos=self)

        self.packages = FlatpakPackages(self)

    def add(self, repo, update=True, override=True):
        try:
            repo.repos = self

            same_name = None
            for name, tmprepo in self.repos.items():
                if repo.url == tmprepo.url:
                    return False
                elif repo.name == name:
                    same_name = tmprepo

            if same_name:
                if override and same_name.url != repo.url:
                    self.flatpak("remote-modify", repo.name, "--url=" + repo.url)
                    same_name.url = repo.url
                    return True
                else:
                    return False

            args = ["remote-add", repo.name, "--if-not-exists"]
            if repo.repo_file:
                args.extend(["--from", repo.repo_file.name])
            else:
                args.extend(["--no-gpg-verify", repo.url])
            self.flatpak(*args, comment="Adding repo %s" % repo.name)
            return True
        finally:
            if update:
                self.packages = FlatpakPackages(self)

    def is_package_installed(self, name, branch=None, arch=None):
        for package in self.packages:
            if name != package.name:
                continue
            if branch and branch != package.branch:
                continue
            if arch and arch != package.arch:
                continue
            return True
        return False


class FlatpakRepo(FlatpakObject):

    def __init__(self, name, desc=None, url=None,
                 repo_file=None, user=True, repos=None):
        FlatpakObject.__init__(self, user=user)

        self.name = name
        self.url = url
        self.desc = desc
        self.repo_file_name = repo_file
        self._repo_file = None
        self.repos = repos
        assert name
        if repo_file and not url:
            repo = configparser.ConfigParser()
            repo.read(self.repo_file.name)
            try:
                self.url = repo["Flatpak Repo"]["Url"]
            except AttributeError:
                self.url = repo.get("Flatpak Repo", "Url")
        else:
            assert url

    @property
    def repo_file(self):
        if self._repo_file:
            return self._repo_file

        if not self.repo_file_name:
            return None

        self._repo_file = tempfile.NamedTemporaryFile(mode="wb")
        self._repo_file.write(urlopen(self.repo_file_name).read())
        self._repo_file.flush()

        return self._repo_file


class FlatpakPackage(FlatpakObject):
    """A flatpak app."""

    def __init__(self, name, branch, repo, arch, user=True, hash=None):
        FlatpakObject.__init__(self, user=user)

        self.name = name
        self.branch = str(branch)
        self.repo = repo
        self.arch = arch

    def __repr__(self):
        return "%s/%s/%s %s" % (self.name, self.arch, self.branch, self.repo.name)

    def __str__(self):
        return "%s/%s/%s" % (self.name, self.arch, self.branch)

    def is_installed(self, branch):
        # Bundles installed from files do not have repositories.
        if not self.repo:
            return True
        return self.repo.repos.is_package_installed(self.name, branch, self.arch)

    def install(self):
        if not self.repo:
            return False

        branch = self.branch
        args = ("install", self.repo.name, self.name, "--reinstall", branch)
        comment = "Installing from " + self.repo.name + " " + self.name + " " + self.arch + " " + branch
        self.flatpak(*args, comment=comment)
        self.repo.repos.packages.update()

@contextmanager
def disable_signals(signals=[signal.SIGINT, signal.SIGTERM, signal.SIGHUP]):
    old_signal_handlers = []

    for disabled_signal in signals:
        handler = signal.getsignal(disabled_signal)
        if handler:
            old_signal_handlers.append((disabled_signal, handler))
        signal.signal(disabled_signal, signal.SIG_IGN)

    yield

    for disabled_signal, previous_handler in old_signal_handlers:
        signal.signal(disabled_signal, previous_handler)


def extract_extra_command_args(args):
    """Takes a list of unparsed args and splits them at '--' to pass to subprocesses."""
    try:
        return args[args.index('--') + 1:]
    except ValueError:
        return []

class WebkitFlatpak:

    @staticmethod
    def load_from_args(args=None, add_help=True):
        self = WebkitFlatpak()

        parser = argparse.ArgumentParser(prog="webkit-flatpak", add_help=add_help)
        general = parser.add_argument_group("General")
        general.add_argument('--verbose', action='store_true', help='Show debug messages')
        general.add_argument('--version', action='store_true', help='Show SDK version', dest="show_version")
        type_group = parser.add_mutually_exclusive_group()
        type_group.add_argument("--debug",
                                help="Compile with Debug configuration, also installs Sdk debug symbols.",
                                dest='build_type', action="store_const", const="Debug")
        type_group.add_argument("--release", help="Compile with Release configuration.",
                                dest='build_type', action="store_const", const="Release")
        general.add_argument('--gtk', action='store_const', dest='platform', const='gtk',
                             help='Setup build directory for the GTK port')
        general.add_argument('--wpe', action='store_const', dest='platform', const='wpe',
                             help=('Setup build directory for the WPE port'))
        general.add_argument("-u", "--update", dest="update",
                             action="store_true",
                             help="Update the SDK")
        general.add_argument("-bdeps", "--build-local-deps", dest="build_local_deps",
                             action="store_true", help="Force rebuilding local dependencies")
        general.add_argument("-q", "--quiet", dest="quiet",
                             action="store_true",
                             help="Do not print anything")
        general.add_argument("-c", "--command",
                             nargs=argparse.REMAINDER,
                             help="The command to run in the sandbox",
                             dest="user_command")
        general.add_argument('--available', action='store_true', dest="check_available", help='Check if required dependencies are available.'),
        general.add_argument("--repo", help="Filesystem absolute path to the Flatpak repository to use", dest="user_repo")

        distributed_build_options = parser.add_argument_group("Distributed building")
        distributed_build_options.add_argument("--use-icecream", dest="use_icecream", help="Use the distributed icecream (icecc) compiler.", action="store_true")
        distributed_build_options.add_argument("-r", "--regenerate-toolchains", dest="regenerate_toolchains", action="store_true",
                                               help="Regenerate IceCC/SCCache standalone toolchain archives")
        distributed_build_options.add_argument("-t", "--sccache-token", dest="sccache_token",
                                               help="sccache authentication token")
        distributed_build_options.add_argument("-s", "--sccache-scheduler", dest="sccache_scheduler",
                                               help="sccache scheduler URL (default: %s)" % DEFAULT_SCCACHE_SCHEDULER)

        debugoptions = parser.add_argument_group("Debugging")
        debugoptions.add_argument("--gdb", nargs="?", help="Activate gdb, passing extra args to it if wanted.")
        debugoptions.add_argument("--gdb-stack-trace", dest="gdb_stack_trace", nargs="?",
                                  help="Dump the stacktrace to stdout. The argument is a timestamp to be parsed by coredumpctl.")
        debugoptions.add_argument("-m", "--coredumpctl-matches", default="", help='Arguments to pass to gdb.')

        buildoptions = parser.add_argument_group("Extra build arguments")
        buildoptions.add_argument("--cmakeargs",
                                  help="One or more optional CMake flags (e.g. --cmakeargs=\"-DFOO=bar -DCMAKE_PREFIX_PATH=/usr/local\")")

        parsing_namespace, self.args = parser.parse_known_args(args=args, namespace=self)
        self.extra_command_args = extract_extra_command_args(parsing_namespace.args)

        if not self.build_type:
            self.build_type = "Release"

        if os.environ.get('CCACHE_PREFIX') == 'icecc':
            self.use_icecream = True

        verbose = os.environ.get('WEBKIT_FLATPAK_SDK_VERBOSE')
        if (not self.verbose) and (verbose is not None):
            self.verbose = verbose != '0'

        configure_logging(logging.DEBUG if self.verbose else logging.INFO)

        if self.user_repo:
            if not os.path.exists(self.user_repo):
                _log.error('User repo at %s is not accessible\n' % self.user_repo)
                sys.exit(1)

        return self

    def __init__(self):
        self.sdk_repo = None
        self.runtime = None
        self.sdk = None
        self.user_repo = None

        self.show_version = False
        self.verbose = False
        self.quiet = False
        self.update = False
        self.args = []
        self.gdb_stack_trace = False

        self.release = False
        self.debug = False

        self.build_local_deps = False

        self.platform = "gtk"
        self.check_available = False
        self.user_command = []

        # debug options
        self.gdb = None
        self.coredumpctl_matches = ""

        # Extra build options
        self.cmakeargs = ""

        self.use_icecream = False
        self.icc_version = {}
        self.regenerate_toolchains = False
        self.sccache_token = ""
        self.sccache_scheduler = DEFAULT_SCCACHE_SCHEDULER

        self.dbus_proxy_process = None

    def __del__(self):
        if self.dbus_proxy_process:
            self.dbus_proxy_process.kill()

    def execute_command(self, args, stdout=None, stderr=None, env=None, keep_signals=True):
        if keep_signals:
            ctx_manager = nullcontext()
        else:
            ctx_manager = disable_signals()
        _log.debug('Running: %s\n' % ' '.join(string_utils.decode(arg) for arg in args))
        result = 0
        with ctx_manager:
            try:
                result = subprocess.check_call(args, stdout=stdout, stderr=stderr, env=env)
            except subprocess.CalledProcessError as err:
                if self.verbose:
                    cmd = ' '.join(string_utils.decode(arg) for arg in err.cmd)
                    message = "'%s' returned a non-zero exit code." % cmd
                    if stderr:
                        with open(stderr.name, 'r') as stderrf:
                            message += " Stderr: %s" % stderrf.read()
                    Console.error_message(message)
                return err.returncode
        return result

    def clean_args(self):
        if self.user_repo:
            os.environ["FLATPAK_USER_DIR"] = FLATPAK_USER_DIR_PATH + ".Local"
        else:
            os.environ["FLATPAK_USER_DIR"] = os.environ.get("WEBKIT_FLATPAK_USER_DIR", FLATPAK_USER_DIR_PATH)
        self.flatpak_build_path = os.environ["FLATPAK_USER_DIR"]
        try:
            os.makedirs(self.flatpak_build_path)
        except OSError as e:
            pass
        _log.debug("Using flatpak user dir: %s" % self.flatpak_build_path)

        self.platform = self.platform.upper()

        if self.gdb is None and '--gdb' in sys.argv:
            self.gdb = True

        self.config_file = os.path.join(self.flatpak_build_path, 'webkit_flatpak_config.json')
        self.sccache_config_file = os.path.join(self.flatpak_build_path, 'sccache.toml')

        self.build_path = get_build_dir(self.platform, self.build_type)
        _log.debug("Building %s port in %s" % (self.platform, self.build_path))

        self.toolchains_directory = os.path.join(self.flatpak_build_path, "Toolchains")
        if not os.path.isdir(self.toolchains_directory):
            os.makedirs(self.toolchains_directory)

        Console.quiet = self.quiet
        self.flatpak_version = check_flatpak()
        if not self.flatpak_version:
            return False

        self._reset_repository()

        try:
            with open(self.config_file) as config:
                json_config = json.load(config)
                self.icc_version = json_config['icecc_version']
        except IOError as e:
            pass

        return True

    def _reset_repository(self):
        url = WEBKIT_SDK_REPO_URL
        repo_file = WEBKIT_SDK_FLATPAK_REPO_URL
        if self.user_repo:
            url = "file://%s" % self.user_repo
            repo_file = None

        self.sdk_repo = FlatpakRepo("webkit-sdk", url=url, repo_file=repo_file)
        self.flathub_repo = FlatpakRepo("flathub", url="https://dl.flathub.org/repo/",
                                        repo_file="https://dl.flathub.org/repo/flathub.flatpakrepo")
        self.repos = FlatpakRepos([self.sdk_repo, self. flathub_repo])

    def setup_builddir(self):
        if os.path.exists(os.path.join(self.flatpak_build_path, "metadata")):
            return True

        if not self.check_installed_packages():
            return False

        self.sdk_repo.flatpak("build-init",
                              self.flatpak_build_path,
                              "org.webkit.Webkit",
                              str(self.sdk),
                              str(self.runtime),
                              self.sdk.branch)

        return True

    def setup_local_deps(self, building):
        if not os.environ.get('WEBKIT_SDK_LOCAL_DEPS'):
            if building:
                _log.debug("$WEBKIT_SDK_LOCAL_DEPS environment variable not set. Skipping local dependencies build")
            return {}

        src_dir = os.path.join(WEBKIT_SOURCE_DIR, 'Tools', 'flatpak', 'local-projects')
        build_dir = os.path.join(DEFAULT_BUILD_ROOT, 'deps-build')
        sandbox_build_dir = convert_webkit_source_path_to_sandbox_path(build_dir)
        if not os.path.exists(os.path.join(build_dir, 'build.ninja')):
            if not building:
                raise RuntimeError('Trying to enter deps-build env from %s but it is not built, make sure to rebuild webkit.', src_dir)

            projects = '-Dsubprojects=%s' % os.environ['WEBKIT_SDK_LOCAL_DEPS']
            options = shlex.split(os.environ.get('WEBKIT_SDK_LOCAL_DEPS_OPTIONS', ''))
            args = ['meson', projects]
            args.extend(options + [convert_webkit_source_path_to_sandbox_path(src_dir), sandbox_build_dir])
            self.run_in_sandbox(*args, building_local_deps=True, start_sccache=False)

        if building:
            Console.message("Building local dependencies from %s ", src_dir)
            if self.run_in_sandbox('meson', 'compile', '-C', sandbox_build_dir, building_local_deps=True, start_sccache=False) != 0:
                raise RuntimeError('Error while building local dependencies.')

        command = ['meson', 'devenv', '-C', sandbox_build_dir, '--dump']
        local_env = self.run_in_sandbox(*command, building_local_deps=True, start_sccache=False, gather_output=True)
        env = {}
        for line in [line for line in local_env.splitlines() if not line.startswith("export")]:
            tokens = line.split("=")
            var_name, contents = tokens[0], "=".join(tokens[1:])
            env[var_name] = contents
        return env

    def _merge_env_variables(self, environment, additional_environment):
        for var_name, value in additional_environment.items():
            if var_name not in environment:
                environment[var_name] = value
            else:
                if var_name.endswith('PATH'):
                    environment[var_name] = "%s:%s" % (environment[var_name], value)
        return environment

    def is_branch_build(self):
        try:
            with open(os.devnull, 'w') as devnull:
                rev_parse = subprocess.check_output(("git", "rev-parse", "--abbrev-ref", "HEAD"), stderr=devnull)
        except subprocess.CalledProcessError:
            # This is likely not a git checkout.
            return False
        git_branch_name = rev_parse.decode("utf-8").strip()
        for option_name in ("branch.%s.webKitBranchBuild" % git_branch_name,
                            "webKitBranchBuild"):
            try:
                with open(os.devnull, 'w') as devnull:
                    output = subprocess.check_output(("git", "config", "--bool", option_name), stderr=devnull).strip()
            except subprocess.CalledProcessError:
                continue

            if output == "true":
                return True
        return False

    def is_build_webkit(self, command):
        return command and "build-webkit" in os.path.basename(command)

    def is_build_jsc(self, command):
        return command and "build-jsc" in os.path.basename(command)

    def setup_a11y_proxy(self):
        # This method might be called more than once, in case of local projects setups for instance.
        # Overriding the xdg-dbus-proxy process actually leads to process leaks. Ensure the process
        # is created at most one time.
        if self.dbus_proxy_process:
            return []
        try:
            output = subprocess.check_output(("gdbus", "call", "-e", "-d", "org.a11y.Bus", "-o", "/org/a11y/bus", "-m", "org.a11y.Bus.GetAddress"))
            a11y_bus_address = re.findall(br"'([^']+)", output)[0]  # Extract string from output from: ('unix:abstract=0000f',)
            _log.debug("Found a11y address {}".format(a11y_bus_address))
        except (subprocess.CalledProcessError, IndexError, FileNotFoundError) as e:
            _log.warning("Failed to get a11y address {}".format(e))
            return []

        dbus_proxy_path = shutil.which("xdg-dbus-proxy")
        if not dbus_proxy_path:
            _log.warning("Failed to find xdg-dbus-proxy. Can't forward a11y bus.")
            return []

        self.socket_dir = tempfile.TemporaryDirectory(prefix="webkit-flatpak-a11y-sockets-")
        self.a11y_socket = tempfile.NamedTemporaryFile(dir=self.socket_dir.name, delete=False)

        try:
            self.dbus_proxy_process = subprocess.Popen((dbus_proxy_path, a11y_bus_address, self.a11y_socket.name, "--talk=org.a11y.Bus"), close_fds=True)
        except (subprocess.CalledProcessError) as e:
            _log.warning("Failed to run xdg-dbus-proxy {}. Can't forward a11y bus.".format(e))
            return []

        return [
            # FIXME: --session-bus is only a workaround for https://github.com/flatpak/flatpak/pull/4630
            "--session-bus",
            "--no-a11y-bus",
            "--filesystem=" + self.socket_dir.name,
            "--env=AT_SPI_BUS_ADDRESS=unix:path=" + self.a11y_socket.name,
        ]

    def run_in_sandbox(self, *args, **kwargs):
        if not self.setup_builddir():
            return 1
        extra_env_vars = kwargs.get("env", {})
        stdout = kwargs.get("stdout", sys.stdout)
        extra_flatpak_args = kwargs.get("extra_flatpak_args", [])
        start_sccache = kwargs.get("start_sccache", True)
        skip_icc = kwargs.get("skip_icc", False)
        building_local_deps = kwargs.get("building_local_deps", False)
        gather_output = kwargs.get("gather_output", False)

        if gather_output:
            start_sccache = False
            skip_icc = True

        if not isinstance(args, list):
            args = list(args)

        sandbox_build_path = os.path.join(SANDBOX_SOURCE_ROOT, BUILD_ROOT_DIR_NAME, self.build_type)
        sandbox_environment = {
            "TEST_RUNNER_INJECTED_BUNDLE_FILENAME": os.path.join(sandbox_build_path, "lib/libTestRunnerInjectedBundle.so"),
            "PATH": "/usr/lib/sdk/llvm14/bin:/usr/bin:/usr/lib/sdk/rust-stable/bin/",
        }

        if not args:
            args.append("bash")

        if args:
            if gather_output:
                command = args[0]
            elif os.path.exists(args[0]):
                command = os.path.normpath(os.path.abspath(args[0]))
                # Take into account the fact that the webkit source dir is remounted inside the sandbox.
                args[0] = convert_webkit_source_path_to_sandbox_path(command)

            if args[0] == "bash":
                args.extend(['--noprofile', '--norc', '-i'])
                sandbox_environment["PS1"] = f"[📦🌐🐱 $FLATPAK_ID {self.platform}@{self.build_type} \\W]\\$ "
            if gather_output:
                building = False
            else:
                building = self.is_build_jsc(args[0]) or self.is_build_webkit(args[0])
        else:
            building = False

        flatpak_command = ["flatpak", "run",
                           "--user",
                           "--die-with-parent",
                           "--filesystem=host",
                           "--allow=devel",
                           "--talk-name=org.gtk.vfs",
                           "--talk-name=org.gtk.vfs.*"]

        flatpak_a11y_args = self.setup_a11y_proxy()
        flatpak_command.extend(flatpak_a11y_args)

        if not gather_output and args and self.is_build_webkit(args[0]) and not self.is_branch_build():
            # Ensure self.build_path exists.
            try:
                os.makedirs(self.build_path)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise e

        share_network_option = "--share=network"

        if self.platform == 'WPE':
            flatpak_command.append(share_network_option)

        if not building:
            flatpak_command.extend([
                "--device=all",
                "--device=dri",
                "--share=ipc",
                "--share=network",
                "--socket=pulseaudio",
                "--socket=session-bus",
                "--socket=system-bus",
                "--socket=wayland",
                "--socket=x11",
                "--system-talk-name=org.a11y.Bus",
                "--system-talk-name=org.freedesktop.GeoClue2",
                "--talk-name=org.freedesktop.Flatpak",
                "--talk-name=org.freedesktop.secrets"
            ])

            sandbox_environment.update({
                "TZ": "America/Los_Angeles",
            })

        env_var_prefixes_to_keep = [
            "G",
            "CCACHE",
            "EGL",
            "GIGACAGE",
            "GTK",
            "ICECC",
            "JSC",
            "MESA",
            "LIBGL",
            "PIPEWIRE",
            "NICE",
            "RUST",
            "SCCACHE",
            "SPA",
            "WAYLAND",
            "WEBKIT",
            "WEBKIT2",
            "WPE",
        ]

        env_var_suffixes_to_keep = [
            "JSC_ARGS",
            "PROCESS_CMD_PREFIX",
            "WEBKIT_ARGS",
        ]

        env_vars_to_keep = [
            "CC",
            "CCACHE_PREFIX",
            "CFLAGS",
            "CXX",
            "CXXFLAGS",
            "DISPLAY",
            "JavaScriptCoreUseJIT",
            "LDFLAGS",
            "MAX_CPU_LOAD",
            "Malloc",
            "NUMBER_OF_PROCESSORS",
            "QML2_IMPORT_PATH",
            "RESULTS_SERVER_API_KEY",
            "SSLKEYLOGFILE",
            "XR_RUNTIME_JSON",
        ]

        def envvar_in_suffixes_to_keep(envvar):
            for env_var in env_var_suffixes_to_keep:
                if envvar.endswith(env_var):
                    return True
            return False

        env_vars = os.environ
        env_vars.update(extra_env_vars)
        for envvar, value in env_vars.items():
            var_tokens = envvar.split("_")
            if var_tokens[0] in env_var_prefixes_to_keep or envvar in env_vars_to_keep or envvar_in_suffixes_to_keep(envvar) or (not os.environ.get('GST_BUILD_PATH') and var_tokens[0] == "GST"):
                sandbox_environment[envvar] = value

        remote_sccache_configs = set(["SCCACHE_REDIS", "SCCACHE_BUCKET", "SCCACHE_MEMCACHED",
                                      "SCCACHE_GCS_BUCKET", "SCCACHE_AZURE_CONNECTION_STRING",
                                      "WEBKIT_USE_SCCACHE"])
        if remote_sccache_configs.intersection(set(os.environ.keys())) and start_sccache and not building_local_deps:
            _log.debug("Enabling network access for the remote sccache")
            flatpak_command.append(share_network_option)

            sccache_environment = {}
            if os.path.isfile(self.sccache_config_file) and not self.regenerate_toolchains and \
               "SCCACHE_CONF" not in os.environ.keys():
                sccache_environment["SCCACHE_CONF"] = convert_webkit_source_path_to_sandbox_path(self.sccache_config_file)

            override_sccache_server_port = os.environ.get("WEBKIT_SCCACHE_SERVER_PORT")
            if override_sccache_server_port:
                _log.debug("Overriding sccache server port to %s" % override_sccache_server_port)
                sccache_environment["SCCACHE_SERVER_PORT"] = override_sccache_server_port

            if building:
                # Spawn the sccache server in background, and avoid recursing here, using a bool keyword.
                _log.debug("Pre-starting the SCCache dist server")
                self.run_in_sandbox("sccache", "--start-server", env=sccache_environment, building_local_deps=building_local_deps,
                                    extra_flatpak_args=[share_network_option], start_sccache=False)

            # Forward sccache server env vars to sccache clients.
            sandbox_environment.update(sccache_environment)

        if self.use_icecream and not skip_icc:
            _log.debug('Enabling the icecream compiler')
            flatpak_command.extend([share_network_option, "--filesystem=home"])

            try:
                n_cores = os.environ["NUMBER_OF_PROCESSORS"]
            except KeyError:
                n_cores = multiprocessing.cpu_count() * 3
                _log.debug('Following icecream recommendation for the number of cores to use: %d' % n_cores)
            toolchain_name = os.environ.get("CC", "gcc")
            default_toolchain = self.icc_version.get(toolchain_name)

            try:
                toolchain_override = os.environ["ICECC_VERSION_OVERRIDE"]
            except KeyError:
                toolchain_path = default_toolchain
            else:
                if not os.path.isfile(toolchain_override):
                    Console.error_message("%s toolchain not found. ICECC_VERSION_OVERRIDE mis-configured?", toolchain_override)
                    return 1

                toolchain_path = convert_webkit_source_path_to_sandbox_path(toolchain_override)

            if not toolchain_path:
                Console.error_message("Toolchains configuration not found. Please run webkit-flatpak -r or set ICECC_VERSION_OVERRIDE to a valid host path")
                return 1

            if "ICECC_VERSION_APPEND" in os.environ:
                extra_toolchain = os.environ["ICECC_VERSION_APPEND"]
                if not os.path.isfile(extra_toolchain):
                    Console.error_message("%s is not a valid IceCC toolchain. ICECC_VERSION_APPEND mis-configured?", extra_toolchain)
                    return 1
                toolchain_path += ","
                toolchain_path += convert_webkit_source_path_to_sandbox_path(extra_toolchain)

            sandbox_environment.update({
                "CCACHE_PREFIX": "icecc",
                "ICECC_TEST_SOCKET": "/run/icecc/iceccd.socket",
                "ICECC_VERSION": toolchain_path,
                "NUMBER_OF_PROCESSORS": n_cores,
            })

        if not building_local_deps and args[0] != "sccache":
            # Merge local dependencies build env vars in sandbox environment, without overriding
            # previously set PATH values.
            local_env = self.setup_local_deps(building)
            sandbox_environment = self._merge_env_variables(sandbox_environment, local_env)

        for envvar, value in sandbox_environment.items():
            flatpak_command.append("--env=%s=%s" % (envvar, value))

        # $WEBKIT_OUTPUTDIR is not forwarded in the build sandbox because the host build path is
        # always bind-mounted to /app in the sandbox.
        env_vars_to_drop = ("WEBKIT_OUTPUTDIR", "LANGUAGE")
        flatpak_env = os.environ.copy()
        for envvar in list(flatpak_env.keys()):
            if envvar.startswith("LC_") or envvar in env_vars_to_drop:
                del flatpak_env[envvar]
                if self.flatpak_version >= (1, 10, 0):
                    flatpak_command.append("--unset-env=%s" % envvar)

        # Avoid 'error: Invalid byte sequence in conversion input' after removing
        # all `LANG` vars.
        flatpak_env["LANG"] = "en_US.UTF-8"

        keep_signals = args[0] != "gdb"
        if not keep_signals:
            module_path = os.path.join(self.build_path, "lib", "libsigaction-disabler.so")
            # Enable module in bwrap child processes.
            extra_flatpak_args.append("--env=WEBKIT_FLATPAK_LD_PRELOAD=%s" % module_path)
            # Enable module in `flatpak run`.
            flatpak_env["LD_PRELOAD"] = module_path

        flatpak_command += extra_flatpak_args + ['--command=%s' % args[0], "org.webkit.Sdk"] + args[1:]

        flatpak_env.update({
            "FLATPAK_BWRAP": os.path.join(SCRIPT_DIR, "webkit-bwrap"),
            "WEBKIT_BUILD_DIR_BIND_MOUNT": "%s:%s" % (sandbox_build_path, self.build_path),
            "WEBKIT_FLATPAK_USER_DIR": os.environ["FLATPAK_USER_DIR"],
        })

        display = os.environ.get("DISPLAY")
        if display:
            flatpak_env["WEBKIT_FLATPAK_DISPLAY"] = display

        # FIXME: Remove duplicate values from the flatpak command.
        command = flatpak_command

        if gather_output:
            return run_sanitized(command, gather_output=True, ignore_stderr=False, env=flatpak_env)

        try:
            return self.execute_command(command, stdout=stdout, env=flatpak_env, keep_signals=keep_signals)
        except KeyboardInterrupt:
            return 0

        return 0

    def main(self):
        if self.check_available:
            return 0

        if not self.clean_args():
            return 1

        if self.show_version:
            print(self.sdk_repo.version("org.webkit.Sdk"))
            return 0

        if self.update:
            flatpak_wrapper = FlatpakObject(True)
            version_before_update = flatpak_wrapper.version("org.webkit.Sdk")
            flatpak_wrapper.flatpak_update()
            regenerate_toolchains = (flatpak_wrapper.version("org.webkit.Sdk") != version_before_update) or not self.check_toolchains_generated()

            # If we have an out-of-date package, simply remove the entire flatpak directory and start over.
            for package in self._get_dependency_packages():
                if package.name.startswith("org.webkit") \
                   and self.repos.is_package_installed(package.name) \
                   and not self.repos.is_package_installed(package.name, branch=SDK_BRANCH):

                    # Cache sccache auth token before removing UserFlatpak.
                    self.acquire_sccache_auth_token_from_config_file()

                    Console.message("New SDK version available, removing local UserFlatpak directory before switching to new version")
                    shutil.rmtree(self.flatpak_build_path)

                    Console.message("Removing webkitpy auto-installed dependencies, new Python runtime might be incompatible with them.")
                    shutil.rmtree(os.path.join(WEBKIT_SOURCE_DIR, 'Tools', 'Scripts', 'libraries', 'autoinstalled'))

                    Console.message("Forcing next WebKit build to re-run CMake")
                    for platform in ('GTK', 'WPE'):
                        for build_type in ('Release', 'Debug'):
                            cache_path = os.path.join(get_build_dir(platform, build_type), 'CMakeCache.txt')
                            if os.path.isfile(cache_path):
                                Console.message("Removing %s", cache_path)
                                os.remove(cache_path)

                            if platform == 'WPE':
                                cog_build_dir = os.path.join(get_build_dir(platform, build_type), 'Tools', 'cog-prefix')
                                if os.path.isdir(cog_build_dir):
                                    Console.message("Removing Cog build directory %s", cog_build_dir)
                                    shutil.rmtree(cog_build_dir)

                    self._reset_repository()
                    break

            for package in self._get_dependency_packages():
                if not self.repos.is_package_installed(package.name):
                    package.install()
                    regenerate_toolchains = True

            print("SDK version: {}".format(self.sdk_repo.version("org.webkit.Sdk")))

        else:
            regenerate_toolchains = self.regenerate_toolchains

        result = self.setup_dev_env()
        if regenerate_toolchains:

            # Toolchains used to be stored in WebKitBuild/Toolchains. Remove this path if found, to save
            # up disk space.
            old_toolchains_path = os.path.join(DEFAULT_BUILD_ROOT, "Toolchains")
            if os.path.isdir(old_toolchains_path):
                Console.message("Purging obsolete toolchains")
                shutil.rmtree(old_toolchains_path)

            if not os.path.isdir(self.toolchains_directory):
                os.makedirs(self.toolchains_directory)

            Console.message("Updating icecc/sccache standalone toolchain archives")
            self.icc_version = {}
            gcc_archive, toolchains = self.pack_toolchain(("gcc", "g++"), {"/usr/bin/c++": "g++",
                                                                           "/usr/bin/cc": "gcc"})
            clang_archive, clang_toolchains = self.pack_toolchain(("clang", "clang++"), {"/usr/bin/clang++": "clang++",
                                                                                          "/usr/bin/clang": "clang"})
            toolchains.extend(clang_toolchains)
            if len(toolchains) > 1:
                self.save_config(toolchains)
                self.purge_unused_toolchains((gcc_archive, clang_archive))
            else:
                Console.error_message("Error generating icecc/sccache standalone toolchain archives")

        return result

    def run(self):
        try:
            return self.main()
        except subprocess.CalledProcessError as error:
            Console.error_message("The following command returned a non-zero exit status: %s\n"
                                  "Output: %s", ' '.join(error.cmd), error.output)
            return error.returncode
        return 0

    def has_environment(self):
        return os.path.exists(self.flatpak_build_path)

    def acquire_sccache_auth_token_from_config_file(self):
        if os.path.isfile(self.sccache_config_file) and not self.sccache_token:
            Console.message("Reusing sccache auth token from old configuration file")
            with open(self.sccache_config_file) as config:
                sccache_config = toml.load(config)
                self.sccache_token = sccache_config['dist']['auth']['token']

    def save_config(self, toolchains):
        with open(self.config_file, 'w') as config:
            json_config = {'icecc_version': self.icc_version}
            json.dump(json_config, config)

        self.acquire_sccache_auth_token_from_config_file()

        if not self.sccache_token:
            Console.message("No authentication token provided. Re-run this with the -t option if an sccache token was provided to you. Skipping sccache configuration for now.")
            return

        with open(self.sccache_config_file, 'w') as config:
            sccache_config = {'dist': {'scheduler_url': self.sccache_scheduler,
                                       'auth': {'type': 'token',
                                                'token': self.sccache_token},
                                       'toolchains': toolchains}}
            toml.dump(sccache_config, config)
            Console.message("Created %s sccache config file. It will automatically be used when building WebKit", self.sccache_config_file)

    def purge_unused_toolchains(self, allow_list):
        for filename in os.listdir(self.toolchains_directory):
            if filename not in allow_list:
                _log.debug("Removing unused toolchain: %s", filename)
                os.remove(os.path.join(self.toolchains_directory, filename))

    def check_toolchains_generated(self):
        found_toolchains = 0
        if os.path.isfile(self.config_file):
            with open(self.config_file, 'r') as config_fd:
                config = json.load(config_fd)
                if 'icecc_version' in config:
                    for compiler in config['icecc_version']:
                        if os.path.isfile(convert_sandbox_path_to_webkit_source_path(config['icecc_version'][compiler])):
                            found_toolchains += 1
        return found_toolchains > 1

    def pack_toolchain(self, compilers, path_mapping):
        compiler_mapping = {}
        for compiler in compilers:
            compiler_mapping[compiler] = self.run_in_sandbox("/usr/bin/which", compiler, gather_output=True)

        with tempfile.NamedTemporaryFile() as tmpfile:
            command = ['icecc', '--build-native']
            command.extend(compiler_mapping.values())
            retcode = self.run_in_sandbox(*command, stdout=tmpfile, cwd=WEBKIT_SOURCE_DIR, skip_icc=True)
            if retcode != 0:
                Console.error_message('Flatpak command "%s" failed with return code %s', " ".join(command), retcode)
                return []
            tmpfile.flush()
            tmpfile.seek(0)
            icc_version_filename, = re.findall(br'.*creating (.*)', tmpfile.read())
            relative_filename = "webkit-sdk-{name}-{filename}".format(name=compilers[0], filename=icc_version_filename.decode())
            archive_filename = os.path.join(self.toolchains_directory, relative_filename)
            os.rename(icc_version_filename, archive_filename)
            archive_sandbox_path = convert_webkit_source_path_to_sandbox_path(archive_filename)
            self.icc_version[compilers[0]] = archive_sandbox_path
            Console.message("Created %s self-contained toolchain archive", archive_filename)

            sccache_toolchains = []
            for (compiler_executable, archive_compiler_executable) in path_mapping.items():
                item = {'type': 'path_override',
                        'compiler_executable': compiler_executable,
                        'archive': archive_sandbox_path,
                        'archive_compiler_executable': compiler_mapping[archive_compiler_executable]}
                sccache_toolchains.append(item)
            return (relative_filename, sccache_toolchains)

    def check_installed_packages(self):
        for package in self._get_dependency_packages():
            if package.name.startswith("org.webkit") and not package.is_installed(SDK_BRANCH):
                Console.error_message("Flatpak package %s not installed. Please update your SDK: Tools/Scripts/update-webkit-flatpak", package)
                return False
        else:
            return True


    def setup_dev_env(self):
        if not os.path.exists(os.path.join(self.flatpak_build_path, "runtime", "org.webkit.Sdk")) or self.update:
            self.install_all()

        if not self.update and not self.check_installed_packages():
            return 1

        if self.gdb or self.gdb_stack_trace:
            return self.run_gdb()
        elif self.user_command:
            program = self.user_command[0]
            if self.is_build_webkit(program) and self.cmakeargs:
                self.user_command.append("--cmakeargs=%s" % self.cmakeargs)

            return self.run_in_sandbox(*self.user_command + self.extra_command_args)
        elif not self.update and not self.build_local_deps and not self.regenerate_toolchains:
            return self.run_in_sandbox()

        return 0

    def _get_dependency_packages(self):
        arch = platform.machine()
        self.runtime = FlatpakPackage("org.webkit.Platform", SDK_BRANCH,
                                      self.sdk_repo, arch)
        self.sdk = FlatpakPackage("org.webkit.Sdk", SDK_BRANCH,
                                  self.sdk_repo, arch)
        packages = [self.runtime, self.sdk]
        packages.append(FlatpakPackage('org.webkit.Sdk.Debug', SDK_BRANCH,
                                       self.sdk_repo, arch))
        packages.append(FlatpakPackage("org.freedesktop.Sdk.Extension.llvm14", SDK_BRANCH,
                                       self.flathub_repo, arch))
        packages.append(FlatpakPackage("org.freedesktop.Sdk.Extension.rust-stable", SDK_BRANCH,
                                       self.flathub_repo, arch))
        packages.append(FlatpakPackage("org.freedesktop.Platform.GL.default", SDK_BRANCH,
                                       self.flathub_repo, arch))
        return packages

    def install_all(self):
        if os.path.exists(os.path.join(self.flatpak_build_path, "runtime", "org.webkit.Sdk")):
            return
        Console.message("Installing %s dependencies in %s", self.build_type, self.flatpak_build_path)
        for package in self._get_dependency_packages():
            if not package.is_installed(SDK_BRANCH):
                package.install()

    def run_gdb(self):
        with disable_signals():
            try:
                subprocess.check_output(['which', 'coredumpctl'])
            except subprocess.CalledProcessError as e:
                Console.message("'coredumpctl' not present on the system, can't run. (%s)\n", e)
                return e.returncode

        # We need access to the host from the sandbox to run.
        with tempfile.NamedTemporaryFile() as coredump:
            with tempfile.NamedTemporaryFile() as stderr:
                if self.gdb_stack_trace:
                    cmd = ["coredumpctl", "--since=%s" % self.gdb_stack_trace, "dump"]
                else:
                    cmd = ["coredumpctl", "dump"] + shlex.split(self.coredumpctl_matches)

                result = self.execute_command(cmd, stdout=coredump, stderr=stderr)
                if result != 0:
                    Console.error_message("coredumpctl failed")
                    with open(stderr.name, 'r') as stderrf:
                        stderr = stderrf.read()
                        Console.error_message(stderr)
                    return result

                with open(stderr.name, 'r') as stderrf:
                    stderr = stderrf.read()

                executable, = re.findall(".*Executable: (.*)", stderr)

                if self.gdb:
                    bargs = ["gdb", executable, "/run/host/%s" % coredump.name]
                    if type(self.gdb) != bool:
                        bargs.extend(shlex.split(self.gdb))
                elif self.gdb_stack_trace:
                    bargs = ["gdb", '-ex', "thread apply all backtrace", '--batch', executable, "/run/host/%s" % coredump.name]

                return self.run_in_sandbox(*bargs)

def is_sandboxed():
    return os.path.exists("/.flatpak-info")


def run_in_sandbox_if_available(args):
    if os.environ.get('WEBKIT_JHBUILD', '0') == '1':
        return None

    os.environ["FLATPAK_USER_DIR"] = os.environ.get("WEBKIT_FLATPAK_USER_DIR", FLATPAK_USER_DIR_PATH)
    if not os.path.isdir(os.environ["FLATPAK_USER_DIR"]):
        return None

    if is_sandboxed():
        return None

    if not check_flatpak(verbose=False):
        return None

    # Filter out flatpakutils args for the app.
    runner_args = []
    app_args = []
    opt_prefix = "--flatpak-"
    for arg in args:
        if arg.startswith(opt_prefix):
            runner_args.append("--%s" % arg[len(opt_prefix):])
        else:
            runner_args.append(arg)
            app_args.append(arg)

    flatpak_runner = WebkitFlatpak.load_from_args(runner_args, add_help=False)
    if not flatpak_runner.clean_args():
        return None

    if not flatpak_runner.has_environment():
        return None

    sys.exit(flatpak_runner.run_in_sandbox(*app_args))
