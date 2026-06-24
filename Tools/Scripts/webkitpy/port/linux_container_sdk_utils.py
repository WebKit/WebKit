# Copyright (C) 2025 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
import os
import stat
import re
import shutil
import socket
import subprocess
import sys


WKDEV_CONTAINER_NAME = 'wkdev-build'
WKDEV_SDK_VERSION_FILENAME = '.wkdev-sdk-version'
WKDEV_SDK_IMAGE_REPOSITORY = 'ghcr.io/igalia/wkdev-sdk'
WEBKIT_CONTAINER_SDK_DOCS_URL = 'https://github.com/Igalia/webkit-container-sdk'

# Signals to Perl callers (via container-sdk-autoenter) that auto-enter
# declined and the caller should continue on the host. Kept distinct from
# typical command exit codes (0, 1, 2, 127).
AUTOENTER_DECLINED_EXIT_CODE = 100

# Mirrors Tools/flatpak/flatpakutils.py's env_var_{prefixes,suffixes}_to_keep /
# env_vars_to_keep, with GST_* added for the GStreamer-driven tests.
_ENV_VAR_PREFIXES_TO_KEEP = (
    'CCACHE', 'COG', 'EGL', 'G', 'GIGACAGE', 'GST', 'GTK', 'ICECC',
    'JSC', 'LIBGL', 'MESA', 'NICE', 'PIPEWIRE', 'PULSE', 'RUST',
    'SCCACHE', 'SPA', 'WAYLAND', 'WEBKIT', 'WEBKIT2', 'WPE',
)

_ENV_VAR_SUFFIXES_TO_KEEP = (
    'JSC_ARGS', 'PROCESS_CMD_PREFIX', 'WEBKIT_ARGS',
)

_ENV_VARS_TO_KEEP = frozenset((
    'AT_SPI_BUS_ADDRESS',
    'BUILD_WEBKIT_PRE_SCRIPT',
    'CC', 'CFLAGS', 'CPPFLAGS', 'CXX', 'CXXFLAGS',
    'DISPLAY',
    'JavaScriptCoreUseJIT',
    'LDFLAGS',
    'MAKEFLAGS',
    'Malloc',
    'MAX_CPU_LOAD',
    'NUMBER_OF_PROCESSORS',
    'QML2_IMPORT_PATH',
    'RESULTS_SERVER_API_KEY',
    'SSLKEYLOGFILE',
    'TERM',
    'USER',
    'USERNAME',
    'XDG_SESSION_TYPE',
    'XR_RUNTIME_JSON',
))

# Variables we never want to forward even if they match the rules above:
# they would clobber container-internal state.
_ENV_VARS_NEVER_FORWARD = frozenset((
    'WEBKIT_CONTAINER_SDK',
    'WEBKIT_CONTAINER_SDK_INSIDE_MOUNT_NAMESPACE',
))


def _env_var_should_be_forwarded(name):
    if name in _ENV_VARS_NEVER_FORWARD:
        return False
    if name in _ENV_VARS_TO_KEEP:
        return True
    if name.split('_', 1)[0] in _ENV_VAR_PREFIXES_TO_KEEP:
        return True
    for suffix in _ENV_VAR_SUFFIXES_TO_KEEP:
        if name.endswith(suffix):
            return True
    return False


def _read_first_line_of(path):
    try:
        with open(path, 'r') as f:
            return f.read().strip() or None
    except OSError:
        return None


def _read_running_sdk_version():
    # Set by the wkdev-sdk Containerfile (`RUN echo "${WKDEV_SDK_VERSION}" > /etc/wkdev-sdk-version`).
    return _read_first_line_of('/etc/wkdev-sdk-version')


def _read_container_name():
    """Return the podman --name of the current container, or None.

    podman writes /run/.containerenv inside every container it spawns; the
    `name=` line holds the value passed to `podman run --name`."""
    try:
        with open('/run/.containerenv', 'r') as f:
            for line in f:
                if line.startswith('name='):
                    return line.split('=', 1)[1].strip().strip('"') or None
    except OSError:
        pass
    return None


def _source_dir():
    return os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))


def _read_pinned_sdk_version(source_dir):
    return _read_first_line_of(os.path.join(source_dir, WKDEV_SDK_VERSION_FILENAME))


def _strip_unix_path_prefix(value):
    if not value:
        return ''
    value = value.strip()
    # D-Bus address: unix:path=/path,key=val,...
    if value.startswith('unix:path='):
        value = value[len('unix:path='):]
        return value.split(',', 1)[0]
    # PulseAudio: unix:/path
    if value.startswith('unix:'):
        return value[len('unix:'):]
    return value


def _get_at_spi_bus_socket_or_dir_and_var(xdg):
    """Find the AT-SPI bus socket, or the directory containing it."""
    def _socket_or_none(addr):
        path = _strip_unix_path_prefix(addr or '')
        try:
            return path if path and stat.S_ISSOCK(os.stat(path).st_mode) else None
        except OSError:
            return None

    # 1. AT_SPI_BUS_ADDRESS env var has precedence if defined
    path = _socket_or_none(os.environ.get('AT_SPI_BUS_ADDRESS'))
    if path:
        return path, 'AT_SPI_BUS_ADDRESS'

    # 2. Ask the session bus
    try:
        out = subprocess.run(
            ['gdbus', 'call', '--session', '--dest', 'org.a11y.Bus',
             '--object-path', '/org/a11y/bus', '--method', 'org.a11y.Bus.GetAddress'],
            capture_output=True, text=True, timeout=5).stdout
        m = re.match(r"\('(.+)',\)\s*$", out.strip())
        if m:
            path = _socket_or_none(m.group(1))
            if path:
                return path, None
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        pass

    # 3 Fallback to usual directory
    xdg_dir = os.path.join(xdg, 'at-spi')
    if os.path.isdir(xdg_dir):
        return xdg_dir, None

    return None, None

def _translate_host_path_to_container(host_path):
    # The wkdev container bind-mounts the host ${HOME} at /host/home/${USER}.
    # Any path under ${HOME} on the host is therefore reachable inside the container
    # by replacing that prefix.
    home = os.environ.get('HOME')
    user = os.environ.get('USER') or os.environ.get('LOGNAME')
    if not home or not user:
        return host_path
    home = os.path.realpath(home)
    real_path = os.path.realpath(host_path)
    if real_path == home or real_path.startswith(home + os.sep):
        return '/host/home/' + user + real_path[len(home):]
    return host_path


def _xdg_runtime_dir():
    return os.environ.get('XDG_RUNTIME_DIR') or '/run/user/{}'.format(os.getuid())


def _xdg_config_home():
    return os.environ.get('XDG_CONFIG_HOME') or os.path.join(os.environ.get('HOME', ''), '.config')


def _container_home_path():
    return os.path.join(os.environ['HOME'], WKDEV_CONTAINER_NAME)


def _bind_mount(src, dst, options='rslave'):
    return ['--mount', 'type=bind,source={},destination={},{}'.format(src, dst, options)]


def _container_hostname():
    # Linux caps hostnames at HOST_NAME_MAX (64) bytes; sethostname(2) returns
    # EINVAL above that, which surfaces from crun as "sethostname: Invalid
    # argument" and aborts container startup. In a Kubernetes pod the host
    # hostname is already up to 63 chars, so blindly appending it overflows.
    hostname = '{}.{}'.format(WKDEV_CONTAINER_NAME, socket.gethostname())
    if len(hostname) > 63:
        hostname = hostname[:63]
    return hostname


def _running_inside_container():
    """True if this process is itself already inside a container. Selects the
    device-passthrough strategy in `_build_podman_run_args`: a nested rootless
    pod's /dev has no /dev/console, so crun cannot create one when we bind the
    host's entire /dev over the container's. Inside a container we instead let
    crun own /dev (so --tty works) and re-expose host devices explicitly."""
    # Cheap marker-file check first; fall back to systemd-detect-virt for
    # minimal pod images that omit it.
    if os.path.exists('/run/.containerenv') or os.path.exists('/.dockerenv'):
        return True
    try:
        virt = subprocess.run(['systemd-detect-virt', '--container'],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL).stdout.decode().strip()
    except FileNotFoundError:
        return False
    return bool(virt) and virt != 'none'


def _build_podman_run_args(pinned_version):
    """Static `podman run` flags for an ephemeral wkdev-build container.

    The caller appends env-var forwards, --workdir, --tty/--interactive, the
    image ref, the inline init wrapper, and the user command."""
    container_home = _container_home_path()
    user = os.environ.get('USER') or os.environ.get('LOGNAME')
    uid = os.getuid()
    gid = os.getgid()
    xdg = _xdg_runtime_dir()
    share_host_pid_ns = os.environ.get('WEBKIT_CONTAINER_SDK_SHARE_HOST_PID_NAMESPACE', '1') != '0'

    args = [
        # Ephemeral: container is removed as soon as the user command exits.
        '--rm',
        # Name the container after the invoking process so `podman ps` shows
        # `wkdev-build-<pid>` instead of a random `tender_bhabha`-style name.
        # We `execvp` into podman, so this PID is also podman's PID for the
        # container's full lifetime -- collision-free across concurrent
        # invocations, and `--rm` frees the name when the command exits.
        '--name', '{}-{}'.format(WKDEV_CONTAINER_NAME, os.getpid()),
        # Pin the OCI runtime so behavior does not vary with the host default
        # (some distros still ship `runc`). `crun` is also required by
        # `--init` below when we run with a private PID namespace.
        '--runtime', 'crun',
        '--hostname', _container_hostname(),
        '--userns', 'keep-id',
        # Run as the invoking host user directly. The tmpfs mount for
        # /run/user/<uid> below is created with the right ownership at mount
        # time, so we no longer need a privileged init phase to chown it.
        '--user', '{}:{}'.format(uid, gid),
        '--security-opt', 'label=disable',
        '--security-opt', 'unmask=ALL',
        '--security-opt', 'seccomp=unconfined',
        # SYS_PTRACE: gdb / lldb attach + perf-events under YAMA ptrace_scope.
        # NET_RAW:    ping and raw-socket tooling (e.g. used by some tests).
        # SYS_ADMIN:  bwrap / Wayland sandboxing inside the SDK.
        '--cap-add=SYS_PTRACE',
        '--cap-add=NET_RAW',
        '--cap-add=SYS_ADMIN',
        '--ulimit', 'host',
        '--pids-limit', '-1',
        '--tmpfs', '/tmp',
        # XDG_RUNTIME_DIR as a per-uid tmpfs with the correct ownership and
        # mode set at mount time. `chown=true` makes podman chown the mount
        # point to the container's user (our invoking host UID via keep-id),
        # so the entrypoint can populate it (Wayland / PipeWire / flatpak
        # symlinks) without a privileged setup step. --tmpfs's option syntax
        # rejects uid=/gid=, which is why we use --mount type=tmpfs here.
        '--mount', 'type=tmpfs,destination=/run/user/{},tmpfs-mode=0700,chown=true'.format(uid),
        '--ipc', 'host',
        '--network', 'host',
        # PID namespace: share with the host by default so coredumpctl works
        # end-to-end. The host's systemd-coredump records crashes against the
        # host PID, so the matching journal entries (bind-mounted in below)
        # only resolve from inside the container when its PIDs are the host's.
        # Same goes for gdb/lldb attach and perf against host processes. Opt
        # out with WEBKIT_CONTAINER_SDK_SHARE_HOST_PID_NAMESPACE=0 to get an
        # isolated PID namespace instead.
        '--pid', 'host' if share_host_pid_ns else 'private',
        # Pull only when the tag is not already cached. .wkdev-sdk-version
        # uses immutable tags, so a registry round-trip per host-wrapper
        # invocation would be wasted work; new tags still pull on first use.
        '--pull=missing',
        '--label', 'io.webkit.container={}'.format(WKDEV_CONTAINER_NAME),
        '--label', 'org.opencontainers.image.version={}'.format(pinned_version),
        '--env', 'WEBKIT_CONTAINER_SDK=1',
        '--env', 'HOST_HOME=/host/home/{}'.format(user),
        '--env', 'XDG_RUNTIME_DIR=/run/user/{}'.format(uid),
    ]
    # tini-style PID 1 forwards signals and reaps zombies, so the user command
    # does not have to play PID 1 itself. `--init` needs the `catatonit` helper
    # on the host and a private PID namespace -- podman refuses to inject a new
    # PID 1 when sharing the host's namespace (where PID 1 already exists),
    # and in that case reparenting to host PID 1 reaps zombies for us anyway.
    if not share_host_pid_ns:
        args += ['--init']
    args += _bind_mount(container_home, '/home/{}'.format(user))
    args += _bind_mount(os.path.realpath(os.environ['HOME']), '/host/home/{}'.format(user))

    # Host config (read-only)
    for src, dst in [('/etc/hosts', '/host/etc/hosts'),
                     ('/etc/localtime', '/etc/localtime'),
                     ('/etc/resolv.conf', '/etc/resolv.conf'),
                     ('/etc/machine-id', '/etc/machine-id')]:
        if os.path.exists(src):
            args += _bind_mount(src, dst, options='ro')

    # Devices: gamepads, GPU, NVIDIA via CDI when available. Binding the host's
    # entire /dev shadows the container's, so under --tty crun cannot create
    # /dev/console from the pty in a nested rootless pod (the pod's /dev has
    # none). Inside a container, let crun own /dev; GPU is passed explicitly.
    if not _running_inside_container():
        args += ['-v', '/dev:/dev:rslave']
    args += ['--mount', 'type=devpts,destination=/dev/pts']
    if os.path.isdir('/run/udev'):
        args += ['-v', '/run/udev:/run/udev']
    if os.path.isdir('/dev/dri'):
        args += ['--device', '/dev/dri']
    if os.path.isdir('/dev/input'):
        args += ['--device', '/dev/input']
    if os.path.exists('/etc/cdi/nvidia.yaml') or os.path.exists('/var/run/cdi/nvidia.yaml'):
        args += ['--device', 'nvidia.com/gpu=all']

    # Display
    if os.path.isdir('/tmp/.X11-unix'):
        args += _bind_mount('/tmp/.X11-unix', '/tmp/.X11-unix')

    # Audio
    pulse_dir = _strip_unix_path_prefix(os.environ.get('PULSE_SERVER'))
    if not pulse_dir:
        pulse_dir = os.path.join(xdg, 'pulse')
    elif os.path.isfile(pulse_dir):
        pulse_dir = os.path.dirname(pulse_dir)
    if os.path.isdir(pulse_dir):
        args += _bind_mount(pulse_dir, pulse_dir)

    # DBus session
    dbus_session = _strip_unix_path_prefix(os.environ.get('DBUS_SESSION_BUS_ADDRESS')) or os.path.join(xdg, 'bus')
    if os.path.exists(dbus_session):
        args += _bind_mount(dbus_session, dbus_session)
        # If defined keep the same value, because it may contain not only the path but also ",guid" values.
        if os.environ.get('DBUS_SESSION_BUS_ADDRESS'):
            args += ['--env', '{}={}'.format('DBUS_SESSION_BUS_ADDRESS', os.environ['DBUS_SESSION_BUS_ADDRESS'])]
        else:
            args += ['--env', 'DBUS_SESSION_BUS_ADDRESS=unix:path={}'.format(dbus_session)]

    # DBus system
    if os.path.exists('/run/dbus/system_bus_socket'):
        args += _bind_mount('/run/dbus/system_bus_socket', '/run/dbus/system_bus_socket')

    # Accessibility (at-spi)
    at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(xdg)
    if at_spi_path:
        args += _bind_mount(at_spi_path, at_spi_path)
        if at_spi_env_var:
            args += ['--env', '{}={}'.format(at_spi_env_var, os.environ[at_spi_env_var])]


    # dconf
    dconf_dir = os.path.join(_xdg_config_home(), 'dconf')
    if os.path.isdir(dconf_dir):
        args += _bind_mount(dconf_dir, dconf_dir)

    # coredumpctl: share the coredump store and journal so `coredumpctl` works
    # inside the container. Crashes inside the container are caught by the
    # host's core_pattern handler (systemd-coredump), which writes to
    # /var/lib/systemd/coredump; without these mounts the files and their
    # journal metadata are invisible to tools running in the container.
    if os.path.isdir('/var/lib/systemd/coredump'):
        args += _bind_mount('/var/lib/systemd/coredump', '/var/lib/systemd/coredump')
    if os.path.isdir('/var/log/journal'):
        args += _bind_mount('/var/log/journal', '/var/log/journal', options='ro,rslave')

    # Host runtime dir is exposed as /host/run so Wayland / PipeWire / flatpak
    # sockets can be symlinked into the container's XDG_RUNTIME_DIR per exec.
    if os.path.isdir(xdg):
        args += _bind_mount(xdg, '/host/run', options='bind-propagation=rslave')

    return args


# The tmpfs mount in `_build_podman_run_args` prepares $XDG_RUNTIME_DIR with
# the right ownership before this script runs, so we can populate it from the
# user without a privileged setup step. WAYLAND_DISPLAY and PIPEWIRE_REMOTE
# come in as positional args ($1, $2) so shell metacharacters in their values
# cannot break out of the quoting.
#
# Kept as a single-line script (`;` between statements, no embedded newlines)
# so that `podman ps` shows it as one truncated `sh -c ...` cell instead of
# wrapping across multiple lines.
_EPHEMERAL_ENTRYPOINT_SCRIPT = (
    'set -e; '
    'wayland_name=$1; pipewire_name=$2; shift 2; '
    # Wayland/PipeWire sockets are addressed by name but the target inode can
    # change across compositor restarts, so always refresh the symlink.
    'for s in "$wayland_name" "$pipewire_name"; do '
    '[ -e "/host/run/$s" ] && ln -sfn "/host/run/$s" "$XDG_RUNTIME_DIR/$s"; '
    '[ -e "/host/run/$s.lock" ] && ln -sfn "/host/run/$s.lock" "$XDG_RUNTIME_DIR/$s.lock"; '
    'done; '
    'for d in .flatpak .flatpak-helper doc; do '
    'mkdir -p "/host/run/$d" 2>/dev/null || true; '
    'ln -sfn "/host/run/$d" "$XDG_RUNTIME_DIR/$d"; '
    'done; '
    'exec "$@"'
)


def maybe_enter_webkit_container_sdk(argv=None):
    """If invoked on the host (outside a wkdev-sdk container), re-execute the
    current command inside an ephemeral wkdev-build container at the SDK
    version pinned in .wkdev-sdk-version. The container is removed (--rm) as
    soon as the command exits, so there is no persistent state to keep in
    sync, no recreate-on-arg-change problem, and no reboot recovery to do.

    When already running inside a wkdev-sdk container, verify the running SDK
    version matches the pinned one and warn loudly if not, but continue. The
    in-container version check fires regardless of
    WEBKIT_CONTAINER_SDK_ENABLE_AUTOENTER so users always learn when the
    running container is out of sync with .wkdev-sdk-version.

    `argv` defaults to `sys.argv` when omitted; pass it explicitly when the
    caller is a wrapper (e.g. container-sdk-autoenter) whose own argv differs
    from the host command we want to re-execute.

    Uses `podman` directly -- no dependency on a host-side webkit-container-sdk
    checkout.
    """
    if argv is None:
        argv = sys.argv
    if not sys.platform.startswith('linux'):
        return

    # Cheap opt-out checks are mirrored in webkitdirs.pm::maybeEnterWebKitContainerSDK
    # so the Perl wrappers can skip the Python fork on the common no-op path.
    # Keep both lists in sync.
    if any(os.environ.get(e) == '1' for e in ('WEBKIT_FLATPAK', 'WEBKIT_JHBUILD', 'WEBKIT_CONTAINER_SDK_INSIDE_MOUNT_NAMESPACE')):
        return

    source_dir = _source_dir()
    pinned_version = _read_pinned_sdk_version(source_dir)
    if not pinned_version:
        return

    # Inside container: version-match check (warn, continue). Performed
    # unconditionally -- the auto-enter opt-in below only gates host-side
    # behavior, since by the time we are inside the container the user has
    # already chosen the SDK they want to use.
    if os.environ.get('WEBKIT_CONTAINER_SDK') == '1':
        running_version = _read_running_sdk_version()
        if running_version and running_version != pinned_version:
            container_name = _read_container_name() or 'wkdev'
            print(
                'WARNING: WebKit Container SDK version mismatch ({} running, {} pinned). '
                'Run `wkdev-update {}` on the host and re-enter to update.'.format(
                    running_version, pinned_version, container_name),
                file=sys.stderr,
            )
        return

    # Auto-enter is opt-in: bots and users flip it on via the environment.
    if os.environ.get('WEBKIT_CONTAINER_SDK_ENABLE_AUTOENTER') != '1':
        return

    # Cross-target builds use their own toolchain wrapper (see webkitdirs.pm's
    # runInCrossTargetEnvironment); don't double-wrap them in the SDK container.
    if os.environ.get('WEBKIT_CROSS_TARGET'):
        return

    # Host side: podman is the only prerequisite.
    if not shutil.which('podman'):
        print(
            "WARNING: 'podman' not found on $PATH; WebKit Container SDK auto-launch disabled, "
            "continuing on the host (see {}).".format(WEBKIT_CONTAINER_SDK_DOCS_URL),
            file=sys.stderr,
        )
        return

    # Container-internal home is bind-mounted from a host directory that must
    # exist before `podman run` starts the container.
    os.makedirs(_container_home_path(), mode=0o750, exist_ok=True)

    image_ref = '{}:{}'.format(WKDEV_SDK_IMAGE_REPOSITORY, pinned_version)
    container_cwd = _translate_host_path_to_container(os.getcwd())
    host_command = os.path.realpath(argv[0])
    container_command = _translate_host_path_to_container(host_command)

    run_cmd = ['podman', 'run'] + _build_podman_run_args(pinned_version)
    run_cmd += ['--workdir', container_cwd, '--detach-keys=']
    if sys.stdin.isatty() and sys.stdout.isatty():
        run_cmd += ['--tty', '--interactive']
    else:
        run_cmd += ['--interactive']
    for var in sorted(os.environ):
        if _env_var_should_be_forwarded(var):
            run_cmd += ['--env', '{}={}'.format(var, os.environ[var])]

    wayland = os.environ.get('WAYLAND_DISPLAY', 'wayland-0')
    pipewire = os.environ.get('PIPEWIRE_REMOTE', 'pipewire-0')
    # Image, then the inline entrypoint (`sh -c <script> wkdev-init <args>`),
    # then the user command and its args. `sh -c` sees the script as $0='wkdev-init'
    # and the following tokens as $1, $2, …; after the script's `shift 2`,
    # "$@" expands to (container_command, *user_args), which `exec "$@"`
    # replaces the shell with.
    run_cmd += [
        image_ref,
        'sh', '-c', _EPHEMERAL_ENTRYPOINT_SCRIPT, 'wkdev-init',
        wayland, pipewire,
        container_command,
    ] + argv[1:]

    print('Launching WebKit Container SDK wkdev-build {}.'.format(pinned_version), file=sys.stderr)
    sys.stdout.flush()
    sys.stderr.flush()
    os.execvp('podman', run_cmd)


def maybe_use_container_sdk_root_dir():
    if not sys.platform.startswith('linux'):
        return

    if os.environ.get('WEBKIT_CONTAINER_SDK_INSIDE_MOUNT_NAMESPACE') == '1':
        return

    if os.environ.get('WEBKIT_JHBUILD') == '1':
        return

    if os.environ.get('WEBKIT_CONTAINER_SDK') != '1':
        print('WARNING: Running outside wkdev-sdk container. For proper testing, use {}'.format(WEBKIT_CONTAINER_SDK_DOCS_URL), file=sys.stderr)
        return

    source_dir = _source_dir()
    wrapper_script = os.path.join(source_dir, 'Tools', 'Scripts', 'container-sdk-rootdir-wrapper')
    assert os.path.isfile(wrapper_script) and os.access(wrapper_script, os.X_OK), 'Error finding container-sdk-rootdir-wrapper'

    if subprocess.call([wrapper_script, '--create-symlink']) != 0:
        print('WARNING: Unable to create symlink at /sdk/webkit. Skipping setting up SDK common root dir feature', file=sys.stderr)
        return

    check_command = ['test', '-f', '/sdk/webkit/Tools/Scripts/build-webkit']
    if subprocess.call([wrapper_script] + check_command) == 0:
        command = sys.argv[0]
        if command.startswith(source_dir):
            command = '/sdk/webkit' + command[len(source_dir):]

        print(f'Running in private mount namespace at /sdk/webkit')
        args = [wrapper_script, command] + sys.argv[1:]
        sys.stdout.flush()
        sys.stderr.flush()
        os.execv(wrapper_script, args)

    print('WARNING: Unable to create /sdk/webkit private mount namespace. Continuing only with symlink support.', file=sys.stderr)
