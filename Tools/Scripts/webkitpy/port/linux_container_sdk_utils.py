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
import shutil
import socket
import subprocess
import sys


WKDEV_CONTAINER_NAME = 'wkdev-build'
WKDEV_SDK_VERSION_FILENAME = '.wkdev-sdk-version'
WKDEV_SDK_IMAGE_REPOSITORY = 'ghcr.io/igalia/wkdev-sdk'
WKDEV_INIT_DONE_FILE = '/run/.wkdev-init-done'
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


def _print_prominent_warning(lines):
    bar = '=' * 78
    print('', file=sys.stderr)
    print(bar, file=sys.stderr)
    for line in lines:
        print(line, file=sys.stderr)
    print(bar, file=sys.stderr)
    print('', file=sys.stderr)


def _read_first_line_of(path):
    try:
        with open(path, 'r') as f:
            return f.read().strip() or None
    except OSError:
        return None


def _read_running_sdk_version():
    # Set by the wkdev-sdk Containerfile (`RUN echo "${WKDEV_SDK_VERSION}" > /etc/wkdev-sdk-version`).
    return _read_first_line_of('/etc/wkdev-sdk-version')


def _source_dir():
    return os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))


def _read_pinned_sdk_version(source_dir):
    return _read_first_line_of(os.path.join(source_dir, WKDEV_SDK_VERSION_FILENAME))


def _strip_unix_path_prefix(value):
    if not value:
        return ''
    value = value.strip()
    prefix = 'unix:path='
    if value.startswith(prefix):
        value = value[len(prefix):]
    return value


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


def _podman_container_info(name):
    """Returns (image, status) for an existing container, or (None, None) if missing."""
    try:
        result = subprocess.run(
            ['podman', 'inspect', '--type', 'container',
             '--format', '{{.ImageName}}|{{.State.Status}}', name],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        if result.returncode != 0:
            return None, None
        parts = result.stdout.decode().strip().split('|', 1)
        if len(parts) != 2:
            return None, None
        return parts[0], parts[1]
    except FileNotFoundError:
        return None, None


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


def _build_podman_create_args(pinned_version):
    image_ref = '{}:{}'.format(WKDEV_SDK_IMAGE_REPOSITORY, pinned_version)
    container_home = _container_home_path()
    user = os.environ.get('USER') or os.environ.get('LOGNAME')
    uid = os.getuid()
    xdg = _xdg_runtime_dir()

    args = [
        '--name', WKDEV_CONTAINER_NAME,
        '--hostname', _container_hostname(),
        '--workdir', '/home/{}'.format(user),
        '--userns', 'keep-id',
        '--user', 'root:root',
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
        # No --pid host: crun rejects sharing the host PID namespace when the
        # container is not creating its own cgroup ("containers not creating
        # Cgroups must create a private PID namespace"). The build container
        # runs `sleep infinity` as its own PID 1 with no systemd inside, so a
        # private PID namespace is fine.
        '--ipc', 'host',
        '--network', 'host',
        '--pull=newer',
        '--label', 'io.webkit.container={}'.format(WKDEV_CONTAINER_NAME),
        '--label', 'org.opencontainers.image.version={}'.format(pinned_version),
        '--env', 'WEBKIT_CONTAINER_SDK=1',
        '--env', 'HOST_HOME=/host/home/{}'.format(user),
        '--env', 'XDG_RUNTIME_DIR=/run/user/{}'.format(uid),
    ]
    args += _bind_mount(container_home, '/home/{}'.format(user))
    args += _bind_mount(os.path.realpath(os.environ['HOME']), '/host/home/{}'.format(user))

    # Host config (read-only)
    for src, dst in [('/etc/hosts', '/host/etc/hosts'),
                     ('/etc/localtime', '/etc/localtime'),
                     ('/etc/resolv.conf', '/etc/resolv.conf'),
                     ('/etc/machine-id', '/etc/machine-id')]:
        if os.path.exists(src):
            args += _bind_mount(src, dst, options='ro')

    # Give the container its own devpts so crun does not fail to chown the
    # caller's host /dev/pts/N when setting up the controlling TTY in rootless
    # mode. Skipped under LXC, which does not permit a nested devpts mount.
    try:
        virt = subprocess.run(['systemd-detect-virt'], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL).stdout.decode().strip()
    except FileNotFoundError:
        virt = ''
    if virt != 'lxc':
        args += ['--mount', 'type=devpts,destination=/dev/pts']

    # Devices: gamepads, GPU, NVIDIA via CDI when available
    args += ['-v', '/dev:/dev:rslave']
    if os.path.isdir('/run/udev'):
        args += ['-v', '/run/udev:/run/udev']
    if os.path.isdir('/dev/dri'):
        args += ['--device', '/dev/dri']
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
        args += ['--env', 'DBUS_SESSION_BUS_ADDRESS=unix:path={}'.format(dbus_session)]

    # DBus system
    if os.path.exists('/run/dbus/system_bus_socket'):
        args += _bind_mount('/run/dbus/system_bus_socket', '/run/dbus/system_bus_socket')

    # Accessibility (at-spi)
    at_spi = _strip_unix_path_prefix(os.environ.get('AT_SPI_BUS_ADDRESS')) or os.path.join(xdg, 'at-spi')
    if os.path.isdir(at_spi):
        args += _bind_mount(at_spi, at_spi)

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

    args += [image_ref, 'sleep', 'infinity']
    return args


def _init_and_sync_container_runtime():
    # Fused into one `podman exec` to avoid an extra round-trip per host
    # wrapper invocation. The init step is idempotent via a /run sentinel
    # (tmpfs, so it naturally resets on container recreation); the sync step
    # mirrors webkit-container-sdk's .wkdev-sync-runtime-state and runs every
    # invocation to symlink the live host Wayland/PipeWire sockets and flatpak
    # helper dirs from /host/run into the container's XDG_RUNTIME_DIR.
    uid = os.getuid()
    gid = os.getgid()
    wayland = os.environ.get('WAYLAND_DISPLAY', 'wayland-0')
    pipewire = os.environ.get('PIPEWIRE_REMOTE', 'pipewire-0')
    # WAYLAND_DISPLAY / PIPEWIRE_REMOTE are forwarded as positional args ($1, $2)
    # instead of interpolated into the script body, so shell metacharacters in
    # their values cannot break out of the quoting.
    script = (
        'set +e\n'
        'init_rc=0\n'
        'if [ ! -f {done} ]; then\n'
        '    (set -e\n'
        '     mkdir -p /run/user/{uid}\n'
        '     chmod 700 /run/user/{uid}\n'
        '     chown {uid}:{gid} /run/user/{uid}\n'
        '     for d in /jhbuild /opt/rust /sdk; do\n'
        '         [ -d "$d" ] && chown -R {uid}:{gid} "$d"\n'
        '     done\n'
        '     touch {done})\n'
        '    init_rc=$?\n'
        'fi\n'
        # Wayland/PipeWire sockets are addressed by name (wayland-0, pipewire-0)
        # but the target inode can change across compositor restarts, so always
        # refresh the symlink. The flatpak helper dirs below are stable; guard
        # those with [ -L ] to skip the mkdir+ln on the steady-state path.
        'for s in "$1" "$2"; do\n'
        '    [ -e "/host/run/$s" ] && ln -sfn "/host/run/$s" "$XDG_RUNTIME_DIR/$s"\n'
        '    [ -e "/host/run/$s.lock" ] && ln -sfn "/host/run/$s.lock" "$XDG_RUNTIME_DIR/$s.lock"\n'
        'done\n'
        'for d in .flatpak .flatpak-helper doc; do\n'
        '    if [ ! -L "$XDG_RUNTIME_DIR/$d" ]; then\n'
        '        mkdir -p "/host/run/$d" 2>/dev/null\n'
        '        ln -sfn "/host/run/$d" "$XDG_RUNTIME_DIR/$d" 2>/dev/null\n'
        '    fi\n'
        'done\n'
        'exit $init_rc\n'
    ).format(uid=uid, gid=gid, done=WKDEV_INIT_DONE_FILE)
    rc = subprocess.call(['podman', 'exec', '--user', 'root', WKDEV_CONTAINER_NAME,
                          'sh', '-c', script, 'wkdev-init', wayland, pipewire],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if rc != 0:
        _print_prominent_warning([
            "WARNING: First-run init for '{}' exited with status {}.".format(WKDEV_CONTAINER_NAME, rc),
            '         Some build paths inside the container may have wrong ownership.',
        ])


def _stop_and_remove_container():
    subprocess.call(['podman', 'rm', '--force', WKDEV_CONTAINER_NAME],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def _create_container(pinned_version):
    os.makedirs(_container_home_path(), mode=0o750, exist_ok=True)

    print("Creating WebKit Container SDK container named '{}' (this may take a while)...".format(WKDEV_CONTAINER_NAME),
          file=sys.stderr)
    create_cmd = ['podman', 'create'] + _build_podman_create_args(pinned_version)
    rc = subprocess.call(create_cmd)
    if rc != 0:
        print("ERROR: Failed to create WebKit Container SDK container named '{}'.".format(WKDEV_CONTAINER_NAME),
              file=sys.stderr)
        sys.exit(rc)


def _ensure_container_ready(pinned_version):
    """Bring the wkdev-build container into 'running' state at the pinned version."""
    image, status = _podman_container_info(WKDEV_CONTAINER_NAME)
    expected_image = '{}:{}'.format(WKDEV_SDK_IMAGE_REPOSITORY, pinned_version)
    if image is None:
        _create_container(pinned_version)
        status = 'created'
    elif image != expected_image:
        _print_prominent_warning([
            "Existing '{}' container uses image '{}',".format(WKDEV_CONTAINER_NAME, image),
            "but {} pins '{}'. Recreating the container.".format(WKDEV_SDK_VERSION_FILENAME, pinned_version),
            "Note: any changes inside the container filesystem will be lost;",
            "the container home directory survives.",
        ])
        _stop_and_remove_container()
        _create_container(pinned_version)
        status = 'created'

    if status != 'running':
        rc = subprocess.call(['podman', 'start', WKDEV_CONTAINER_NAME], stdout=subprocess.DEVNULL)
        if rc != 0:
            print("ERROR: Failed to start container '{}'.".format(WKDEV_CONTAINER_NAME), file=sys.stderr)
            sys.exit(rc)


def maybe_enter_webkit_container_sdk(argv=None):
    """If invoked on the host (outside a wkdev-sdk container), re-execute the
    current command inside the 'wkdev-build' container, creating the container
    on first use using the version pinned in .wkdev-sdk-version.

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
            _print_prominent_warning([
                'WARNING: WebKit Container SDK version mismatch.',
                '         Running container SDK version: {}'.format(running_version),
                '         Pinned by .wkdev-sdk-version:  {}'.format(pinned_version),
                '         Re-run any wrapper script (build-webkit, run-webkit-tests, ...)',
                '         from the host to recreate the container at the pinned version.',
                '         Continuing with the current container.',
            ])
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
        _print_prominent_warning([
            "WARNING: 'podman' was not found on $PATH.",
            '         WebKit Container SDK auto-launch is disabled; continuing on the host.',
            '         Builds and tests may not work correctly outside the SDK.',
            '         See {}'.format(WEBKIT_CONTAINER_SDK_DOCS_URL),
        ])
        return

    _ensure_container_ready(pinned_version)
    _init_and_sync_container_runtime()

    container_cwd = _translate_host_path_to_container(os.getcwd())
    host_command = os.path.realpath(argv[0])
    container_command = _translate_host_path_to_container(host_command)

    exec_cmd = [
        'podman', 'exec',
        '--user', '{}:{}'.format(os.getuid(), os.getgid()),
        '--workdir', container_cwd,
        '--detach-keys=',
    ]
    if sys.stdin.isatty() and sys.stdout.isatty():
        exec_cmd += ['--tty', '--interactive']
    else:
        exec_cmd += ['--interactive']
    for var in sorted(os.environ):
        if _env_var_should_be_forwarded(var):
            exec_cmd += ['--env', '{}={}'.format(var, os.environ[var])]
    exec_cmd += [WKDEV_CONTAINER_NAME, container_command] + argv[1:]

    print('Running inside WebKit Container SDK wkdev-sdk {}.'.format(pinned_version), file=sys.stderr)
    sys.stdout.flush()
    sys.stderr.flush()
    os.execvp('podman', exec_cmd)


def maybe_use_container_sdk_root_dir():
    if not sys.platform.startswith('linux'):
        return

    if os.environ.get('WEBKIT_CONTAINER_SDK_INSIDE_MOUNT_NAMESPACE') == '1':
        return

    if any(os.environ.get(e) == '1' for e in ('WEBKIT_FLATPAK', 'WEBKIT_JHBUILD')):
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
