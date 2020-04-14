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

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.port.factory import PortFactory
from webkitpy.common.system.logutils import configure_logging

try:
    from urllib.parse import urlparse  # pylint: disable=E0611
except ImportError:
    from urlparse import urlparse

try:
    from urllib.request import urlopen  # pylint: disable=E0611
except ImportError:
    from urllib2 import urlopen

FLATPAK_REQ = [
    ("flatpak", "1.4.4"),
]

FLATPAK_VERSION = {}

scriptdir = os.path.abspath(os.path.dirname(__file__))
_log = logging.getLogger(__name__)

FLATPAK_USER_DIR_PATH = os.path.realpath(os.path.join(scriptdir, "../../WebKitBuild", "UserFlatpak"))

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

def check_flatpak(verbose=True):
    # Flatpak is only supported on Linux.
    if not sys.platform.startswith("linux"):
        return False

    for app, required_version in FLATPAK_REQ:
        try:
            output = subprocess.check_output([app, "--version"])
        except (subprocess.CalledProcessError, OSError):
            if verbose:
                Console.error_message("You need to install %s >= %s"
                                      " to be able to use the '%s' script.\n\n"
                                      "You can find some informations about"
                                      " how to install it for your distribution at:\n"
                                      "    * https://flatpak.org/%s\n", app, required_version,
                                      sys.argv[0])
            return False

        def comparable_version(version):
            return tuple(map(int, (version.split("."))))

        version = output.decode("utf-8").split(" ")[1].strip("\n")
        current = comparable_version(version)
        FLATPAK_VERSION[app] = current
        if current < comparable_version(required_version):
            Console.error_message("%s %s required but %s found. Please update and try again\n",
                                  app, required_version, version)
            return False

    return True


class FlatpakObject:

    def __init__(self, user):
        self.user = user

    def flatpak(self, command, *args, **kwargs):
        show_output = kwargs.pop("show_output", False)
        comment = kwargs.pop("comment", None)
        if comment:
            Console.message(comment)

        command = ["flatpak", command]
        res = subprocess.check_output(command + ["--help"]).decode("utf-8")
        if self.user and "--user" in res:
            command.append("--user")
        if "--assumeyes" in res:
            command.append("--assumeyes")
        command.extend(args)

        _log.debug("Executing %s" % ' '.join(command))
        if not show_output:
            return subprocess.check_output(command).decode("utf-8")

        return subprocess.check_call(command)


class FlatpakPackages(FlatpakObject):

    def __init__(self, repos, user=True):
        FlatpakObject.__init__(self, user=user)

        self.repos = repos

        packs = []
        out = self.flatpak("list", "--columns=application,arch,branch,origin", "-a")
        package_defs = [line for line in out.split("\n") if line]
        for package_def in package_defs:
            name, arch, branch, origin = package_def.split("\t")

            # If installed from a file, the package is in no repo
            repo = self.repos.repos.get(origin)

            packs.append(FlatpakPackage(name, branch, repo, arch))

        self.packages = packs

    def __iter__(self):
        for package in self.packages:
            yield package


class FlatpakRepos(FlatpakObject):

    def __init__(self, user=True):
        FlatpakObject.__init__(self, user=user)
        self.repos = {}
        self.update()

    def update(self):
        self.repos = {}
        out = self.flatpak("remote-list", "--columns=name,title,url")
        remotes = [line for line in out.split("\n") if line]
        for remote in remotes:
            name, title, url = remote.split("\t")
            parsed_url = urlparse(url)
            if not parsed_url.scheme:
                Console.message("No valid URI found for: %s", remote)
                continue

            self.repos[name] = FlatpakRepo(name=name, url=url, desc=title, repos=self)

        self.packages = FlatpakPackages(self)

    def add(self, repo, override=True):
        same_name = None
        for name, tmprepo in self.repos.items():
            if repo.url == tmprepo.url:
                return tmprepo
            elif repo.name == name:
                same_name = tmprepo

        if same_name:
            if override:
                self.flatpak("remote-modify", repo.name, "--url=" + repo.url,
                             comment="Setting repo %s URL from %s to %s"
                             % (repo.name, same_name.url, repo.url))
                same_name.url = repo.url

                return same_name
            else:
                return None
        else:
            self.flatpak("remote-add", repo.name, "--from", repo.repo_file.name,
                         "--if-not-exists",
                         comment="Adding repo %s" % repo.name)

        repo.repos = self
        return repo


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

        self._app_registry = {}
        output = self.flatpak("list", "--columns=application,branch,origin")
        for line in output.splitlines():
            name, branch, origin = line.split("\t")
            if origin != self.name:
                continue
            self._app_registry[name] = branch

    def is_app_installed(self, name, branch=None):
        if branch:
            try:
                return self._app_registry[name] == branch
            except KeyError:
                return False
        else:
            return name in self._app_registry.keys()

    @property
    def repo_file(self):
        if self._repo_file:
            return self._repo_file

        assert self.repo_file_name
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
        if not self.repo:
            # Bundle installed from file
            return True

        for package in self.repo.repos.packages:
            if package.name == self.name and package.branch == branch and package.arch == self.arch:
                return True

        return False

    def install(self):
        if not self.repo:
            return False

        branch = self.branch
        args = ("install", self.repo.name, self.name, "--reinstall", branch)
        comment = "Installing from " + self.repo.name + " " + self.name + " " + self.arch + " " + branch
        self.flatpak(*args, show_output=True, comment=comment)

    def update(self):
        if not self.is_installed(self.branch):
            return self.install()

        comment = "Updating %s" % self.name
        self.flatpak("update", self.name, self.branch, show_output=True, comment=comment)


@contextmanager
def disable_signals(signals=[signal.SIGINT]):
    old_signal_handlers = []

    for disabled_signal in signals:
        old_signal_handlers.append((disabled_signal, signal.getsignal(disabled_signal)))
        signal.signal(disabled_signal, signal.SIG_IGN)

    yield

    for disabled_signal, previous_handler in old_signal_handlers:
        signal.signal(disabled_signal, previous_handler)


class WebkitFlatpak:

    @staticmethod
    def load_from_args(args=None, add_help=True):
        self = WebkitFlatpak()

        parser = argparse.ArgumentParser(prog="webkit-flatpak", add_help=add_help)
        general = parser.add_argument_group("General")
        general.add_argument('--verbose', action='store_true',
                             help='Show debug message')
        general.add_argument("--debug",
                             help="Compile with Debug configuration, also installs Sdk debug symbols.",
                             action="store_true")
        general.add_argument("--release", help="Compile with Release configuration.", action="store_true")
        general.add_argument('--gtk', action='store_const', dest='platform', const='gtk',
                             help='Setup build directory for the GTK port')
        general.add_argument('--wpe', action='store_const', dest='platform', const='wpe',
                             help=('Setup build directory for the WPE port'))
        general.add_argument("-u", "--update", dest="update",
                             action="store_true",
                             help="Update the SDK")
        general.add_argument("-bgst", "--build-gst", dest="build_gst",
                             action="store_true",
                             help="Force rebuilding gst-build, repository path is defined by the `GST_BUILD_PATH` environment variable.")
        general.add_argument("-q", "--quiet", dest="quiet",
                             action="store_true",
                             help="Do not print anything")
        general.add_argument("-c", "--command",
                             nargs=argparse.REMAINDER,
                             help="The command to run in the sandbox",
                             dest="user_command")
        general.add_argument('--available', action='store_true', dest="check_available", help='Check if required dependencies are available.'),
        general.add_argument("--use-icecream", dest="use_icecream", help="Use the distributed icecream (icecc) compiler.", action="store_true")
        general.add_argument("-r", "--regenerate-toolchains", dest="regenerate_toolchains", action="store_true",
                             help="Regenerate IceCC distribuable toolchain archives")

        debugoptions = parser.add_argument_group("Debugging")
        debugoptions.add_argument("--gdb", nargs="?", help="Activate gdb, passing extra args to it if wanted.")
        debugoptions.add_argument("--gdb-stack-trace", dest="gdb_stack_trace", nargs="?",
                                  help="Dump the stacktrace to stdout. The argument is a timestamp to be parsed by coredumpctl.")
        debugoptions.add_argument("-m", "--coredumpctl-matches", default="", help='Arguments to pass to gdb.')

        buildoptions = parser.add_argument_group("Extra build arguments")
        buildoptions.add_argument("--cmakeargs",
                                  help="One or more optional CMake flags (e.g. --cmakeargs=\"-DFOO=bar -DCMAKE_PREFIX_PATH=/usr/local\")")

        _, self.args = parser.parse_known_args(args=args, namespace=self)

        if os.environ.get('CCACHE_PREFIX') == 'icecc':
            self.use_icecream = True

        return self

    def __init__(self):
        self.sdk_repo = None
        self.runtime = None
        self.sdk = None

        self.verbose = False
        self.quiet = False
        self.update = False
        self.args = []
        self.gdb_stack_trace = False

        self.release = False
        self.debug = False
        self.source_root = os.path.normpath(os.path.abspath(os.path.join(scriptdir, '../../')))
        # Where the source folder is mounted inside the sandbox.
        self.sandbox_source_root = "/app/webkit"

        self.build_gst = False

        self.sdk_branch = "0.2"
        self.platform = "gtk"
        self.build_type = "Release"
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

    def execute_command(self, args, stdout=None, stderr=None):
        _log.debug('Running in sandbox: %s\n' % ' '.join(args))
        result = 0
        try:
            result = subprocess.check_call(args, stdout=stdout, stderr=stderr)
        except subprocess.CalledProcessError as err:
            if self.verbose:
                cmd = ' '.join(err.cmd)
                message = "'%s' returned a non-zero exit code." % cmd
                if stderr:
                    with open(stderr.name, 'r') as stderrf:
                        message += " Stderr: %s" % stderrf.read()
                Console.error_message(message)
            return err.returncode
        return result

    def clean_args(self):
        os.environ["FLATPAK_USER_DIR"] = os.environ.get("WEBKIT_FLATPAK_USER_DIR", FLATPAK_USER_DIR_PATH)
        try:
            os.makedirs(os.environ["FLATPAK_USER_DIR"])
        except OSError as e:
            pass

        configure_logging(logging.DEBUG if self.verbose else logging.INFO)
        _log.debug("Using flatpak user dir: %s" % os.environ["FLATPAK_USER_DIR"])

        if not self.debug and not self.release:
            factory = PortFactory(SystemHost())
            port = factory.get(self.platform)
            self.debug = port.default_configuration() == "Debug"

        self.build_type = "Debug" if self.debug else "Release"
        self.platform = self.platform.upper()

        if self.gdb is None and '--gdb' in sys.argv:
            self.gdb = True

        self.flatpak_build_path = os.environ["FLATPAK_USER_DIR"]

        self.build_root = os.path.join(self.source_root, 'WebKitBuild')
        self.build_path = os.path.join(self.build_root, self.platform, self.build_type)
        self.config_file = os.path.join(self.flatpak_build_path, 'webkit_flatpak_config.json')

        Console.quiet = self.quiet
        if not check_flatpak():
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
        self.repos = FlatpakRepos()
        self.sdk_repo = self.repos.add(
            FlatpakRepo("webkit-sdk",
                        url="https://software.igalia.com/webkit-sdk-repo/",
                        repo_file="https://software.igalia.com/flatpak-refs/webkit-sdk.flatpakrepo")
        )

    def setup_builddir(self, **kwargs):
        if os.path.exists(os.path.join(self.flatpak_build_path, "metadata")):
            return

        self.sdk_repo.flatpak("build-init",
                              self.flatpak_build_path,
                              "org.webkit.Webkit",
                              str(self.sdk),
                              str(self.runtime),
                              self.sdk.branch,
                              show_output="stdout" in kwargs.keys())

    def setup_gstbuild(self, building):
        gst_dir = os.environ.get('GST_BUILD_PATH')
        if not gst_dir:
            if building:
                Console.warning_message("$GST_BUILD_PATH environment variable not set. Skipping gst-build\n")
            return []

        if not os.path.exists(os.path.join(gst_dir, 'gst-env.py')):
            raise RuntimeError('GST_BUILD_PATH set to %s but it doesn\'t seem to be a valid `gst-build` checkout.' % gst_dir)

        gst_builddir = os.path.join(self.sandbox_source_root, "WebKitBuild", 'gst-build')
        if not os.path.exists(os.path.join(self.build_root, 'gst-build', 'build.ninja')):
            if not building:
                raise RuntimeError('Trying to enter gst-build env from %s but it is not built, make sure to rebuild webkit.' % gst_dir)

            args = ['meson', ]
            extra_args = os.environ.get('GST_BUILD_ARGS', '')
            args.extend(shlex.split(extra_args) + [gst_dir, gst_builddir])
            Console.message("Running %s ", ' '.join(args))
            self.run_in_sandbox(*args, building_gst=True)

        if not building:
            return [os.path.join(gst_dir, 'gst-env.py'), '--builddir', gst_builddir, '--srcdir', gst_dir]

        Console.message("Building `gst-build` %s ", gst_dir)
        if self.run_in_sandbox('ninja', '-C', gst_builddir, building_gst=True) != 0:
            raise RuntimeError('Error while building gst-build.')

        return [os.path.join(gst_dir, 'gst-env.py'), '--builddir', gst_builddir, '--srcdir', gst_dir]

    def is_branch_build(self):
        git_branch_name = subprocess.check_output(("git", "rev-parse", "--abbrev-ref", "HEAD")).decode("utf-8").strip()
        for option_name in ("branch.%s.webKitBranchBuild" % git_branch_name,
                            "webKitBranchBuild"):
            try:
                output = subprocess.check_output(("git", "config", "--bool", option_name)).strip()
            except subprocess.CalledProcessError:
                continue

            if output == "true":
                return True
        return False

    def run_in_sandbox(self, *args, **kwargs):
        self.setup_builddir(stdout=kwargs.get("stdout", sys.stdout))
        cwd = kwargs.pop("cwd", None)
        extra_env_vars = kwargs.pop("env", {})
        stdout = kwargs.pop("stdout", sys.stdout)
        extra_flatpak_args = kwargs.pop("extra_flatpak_args", [])

        if not isinstance(args, list):
            args = list(args)

        if args:
            if os.path.exists(args[0]):
                command = os.path.normpath(os.path.abspath(args[0]))
                # Take into account the fact that the webkit source dir is remounted inside the sandbox.
                args[0] = command.replace(self.source_root, self.sandbox_source_root)
            if args[0].endswith("build-webkit"):
                args.append("--prefix=/app")
            building = os.path.basename(args[0]).startswith("build")
        else:
            building = False

        sandbox_build_path = os.path.join(self.sandbox_source_root, "WebKitBuild", self.build_type)
        # FIXME: Using the `run` flatpak command would be better, but it doesn't
        # have a --bind-mount option.
        flatpak_command = ["flatpak", "build",
                           "--die-with-parent",
                           "--talk-name=org.a11y.Bus",
                           "--talk-name=org.gtk.vfs",
                           "--talk-name=org.gtk.vfs.*",
                           "--bind-mount=/run/shm=/dev/shm",
                           # Access to /run/host is required by the crash log reporter.
                           "--bind-mount=/run/host/%s=%s" % (tempfile.gettempdir(), tempfile.gettempdir()),
                           # flatpak build doesn't expose a --socket option for
                           # white-listing the systemd journal socket. So
                           # white-list it in /run, hoping this is the right
                           # path.
                           "--bind-mount=/run/systemd/journal=/run/systemd/journal",
                           "--bind-mount=%s=%s" % (self.sandbox_source_root, self.source_root)]

        if args and args[0].endswith("build-webkit") and not self.is_branch_build():
            # Ensure self.build_path exists.
            try:
                os.makedirs(self.build_path)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise e

        # We mount WebKitBuild/PORTNAME/BuildType to /app/webkit/WebKitBuild/BuildType
        # so we can build WPE and GTK in a same source tree.
        # The bind-mount is always needed, excepted during the initial setup (SDK install/updates).
        if os.path.isdir(self.build_path):
            flatpak_command.append("--bind-mount=%s=%s" % (sandbox_build_path, self.build_path))

        forwarded = {
            "WEBKIT_TOP_LEVEL": "/app/",
            "TEST_RUNNER_INJECTED_BUNDLE_FILENAME": os.path.join(sandbox_build_path, "lib/libTestRunnerInjectedBundle.so"),
        }

        if not building:
            flatpak_command.extend([
                "--device=all",
                "--device=dri",
                "--filesystem=host",
                "--share=ipc",
                "--share=network",
                "--socket=pulseaudio",
                "--socket=system-bus",
                "--socket=wayland",
                "--socket=x11",
                "--system-talk-name=org.a11y.Bus",
                "--system-talk-name=org.freedesktop.GeoClue2",
                "--talk-name=org.a11y.Bus",
                "--talk-name=org.freedesktop.Flatpak"
            ])

            forwarded.update({
                "TZ": "PST8PDT",
                "LANG": "en_US.UTF-8"
            })

        env_var_prefixes_to_keep = [
            "G",
            "GIGACAGE",
            "GST",
            "GTK",
            "ICECC",
            "JSC",
            "SCCACHE",
            "WEBKIT",
            "WEBKIT2",
            "WPE",
        ]

        env_var_suffixes_to_keep = [
            "JSC_ARGS",
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
            "LANG",
            "LDFLAGS",
            "Malloc",
            "GPU_PROCESS_CMD_PREFIX",
            "NETWORK_PROCESS_CMD_PREFIX",
            "NUMBER_OF_PROCESSORS",
            "PLUGIN_PROCESS_CMD_PREFIX",
            "QML2_IMPORT_PATH",
            "WAYLAND_DISPLAY",
            "WAYLAND_SOCKET",
            "WEB_PROCESS_CMD_PREFIX"
        ]

        env_vars = os.environ
        env_vars.update(extra_env_vars)
        for envvar, value in env_vars.items():
            var_tokens = envvar.split("_")
            if var_tokens[0] in env_var_prefixes_to_keep or envvar in env_vars_to_keep or var_tokens[-1] in env_var_suffixes_to_keep:
                forwarded[envvar] = value

        share_network_option = "--share=network"
        remote_sccache_configs = set(["SCCACHE_REDIS", "SCCACHE_BUCKET", "SCCACHE_MEMCACHED",
                                      "SCCACHE_GCS_BUCKET", "SCCACHE_AZURE_CONNECTION_STRING",
                                      "WEBKIT_USE_SCCACHE"])
        if remote_sccache_configs.intersection(set(os.environ.keys())):
            _log.debug("Enabling network access for the remote sccache")
            flatpak_command.append(share_network_option)

        if self.use_icecream and not self.regenerate_toolchains:
            _log.debug('Enabling the icecream compiler')
            if share_network_option not in flatpak_command:
                flatpak_command.append(share_network_option)
            flatpak_command.append("--bind-mount=/var/run/icecc=/var/run/icecc")

            n_cores = multiprocessing.cpu_count() * 3
            _log.debug('Following icecream recommendation for the number of cores to use: %d' % n_cores)
            toolchain_name = os.environ.get("CC", "gcc")
            toolchain_path = self.icc_version[toolchain_name]
            if not os.path.isfile(toolchain_path):
                Console.error_message("%s is not a valid IceCC toolchain. Please run webkit-flatpak -r")
                return 1
            forwarded.update({
                "CCACHE_PREFIX": "icecc",
                "ICECC_VERSION": toolchain_path,
                "NUMBER_OF_PROCESSORS": n_cores,
            })

        for envvar, value in forwarded.items():
            flatpak_command.append("--env=%s=%s" % (envvar, value))

        gst_env = []
        if not kwargs.get('building_gst'):
            gst_env = self.setup_gstbuild(building)

        flatpak_command += extra_flatpak_args + [self.flatpak_build_path] + gst_env + args

        try:
            return self.execute_command(flatpak_command, stdout=stdout)
        except KeyboardInterrupt:
            return 0

        return 0

    def main(self):
        if self.check_available:
            return 0

        if not self.clean_args():
            return 1

        if self.update:
            Console.message("Updating Flatpak %s environment" % self.build_type)
            repo = self.sdk_repo
            repo.flatpak("update")
            for package in self._get_packages():
                if package.name.startswith("org.webkit") and repo.is_app_installed(package.name) \
                   and not repo.is_app_installed(package.name, branch=self.sdk_branch):
                    Console.message("New SDK version available, removing local UserFlatpak directory before switching to new version")
                    shutil.rmtree(self.flatpak_build_path)
                    self._reset_repository()
                    break
                elif not repo.is_app_installed(package.name):
                    package.install()

        return self.setup_dev_env()

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

    def save_config(self):
        with open(self.config_file, 'w') as config:
            json_config = {'icecc_version': self.icc_version}
            json.dump(json_config, config)

    def setup_icecc(self, name):
        with tempfile.NamedTemporaryFile() as tmpfile:
            toolchain_path = "/usr/bin/%s" % name
            self.run_in_sandbox('icecc', '--build-native', toolchain_path, stdout=tmpfile, cwd=self.source_root)
            tmpfile.flush()
            tmpfile.seek(0)
            icc_version_filename, = re.findall(r'.*creating (.*)', tmpfile.read())
            toolchains_directory = os.path.join(self.build_root, "Toolchains")
            if not os.path.isdir(toolchains_directory):
                os.makedirs(toolchains_directory)
            archive_filename = os.path.join(toolchains_directory, "webkit-sdk-%s-%s" % (name, icc_version_filename))
            os.rename(icc_version_filename, archive_filename)
            self.icc_version[name] = archive_filename
            Console.message("Created %s self-contained toolchain archive", archive_filename)

    def setup_dev_env(self):
        if not os.path.exists(os.path.join(self.flatpak_build_path, "runtime", "org.webkit.Sdk")) or self.update:
            self.install_all()
            regenerate_toolchains = True
        else:
            regenerate_toolchains = self.regenerate_toolchains

        if regenerate_toolchains:
            self.icc_version = {}
            self.setup_icecc("gcc")
            self.setup_icecc("clang")
            self.save_config()

        if not self.update:
            for package in self._get_packages():
                if package.name.startswith("org.webkit") and not package.is_installed(self.sdk_branch):
                    Console.error_message("Flatpak package %s not installed. Please update your SDK: Tools/Scripts/update-webkit-flatpak", package)
                    return 1

        if self.gdb or self.gdb_stack_trace:
            return self.run_gdb()
        elif self.user_command:
            program = self.user_command[0]
            if program.endswith("build-webkit"):
                Console.message("Building webkit")
                if self.cmakeargs:
                    self.user_command.append("--cmakeargs=%s" % self.cmakeargs)

            return self.run_in_sandbox(*self.user_command)
        elif not self.update and not self.build_gst:
            return self.run_in_sandbox()

        return 0

    def _get_packages(self):
        self.runtime = FlatpakPackage("org.webkit.Platform", self.sdk_branch,
                                      self.sdk_repo, "x86_64")
        self.sdk = FlatpakPackage("org.webkit.Sdk", self.sdk_branch,
                                  self.sdk_repo, "x86_64")
        packages = [self.runtime, self.sdk]

        # FIXME: For unknown reasons, the GL extension needs to be explicitely
        # installed for Flatpak 1.2.x to be able to make use of it. Seems like
        # it's not correctly inheriting it from the SDK.
        self.flathub_repo = self.repos.add(
            FlatpakRepo("flathub", repo_file="https://dl.flathub.org/repo/flathub.flatpakrepo")
        )
        gl_extension = FlatpakPackage("org.freedesktop.Platform.GL.default", "19.08",
                                      self.flathub_repo, "x86_64")
        packages.append(gl_extension)

        if self.debug:
            sdk_debug = FlatpakPackage('org.webkit.Sdk.Debug', self.sdk_branch,
                                       self.sdk_repo, "x86_64")
            packages.append(sdk_debug)
        return packages

    def install_all(self):
        if os.path.exists(os.path.join(self.flatpak_build_path, "runtime", "org.webkit.Sdk")):
            return
        Console.message("Installing %s dependencies in %s", self.build_type, self.flatpak_build_path)
        for package in self._get_packages():
            if not package.is_installed(self.sdk_branch):
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

    if not os.path.isdir(FLATPAK_USER_DIR_PATH):
        return None

    if is_sandboxed():
        return None

    if not check_flatpak(verbose=False):
        return None

    flatpak_runner = WebkitFlatpak.load_from_args(args, add_help=False)
    if not flatpak_runner.clean_args():
        return None

    if not flatpak_runner.has_environment():
        return None

    sys.exit(flatpak_runner.run_in_sandbox(*args))
