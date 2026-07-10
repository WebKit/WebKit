#  Copyright (c) 2014 Canon Inc. All rights reserved.
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1.  Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#  2.  Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#  3.  Neither the name of Canon Inc. nor the names of
#      its contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#  THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
#  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#  DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
#  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import errno
import json
import logging
import os
import socket
import sys
import time

from webkitpy.layout_tests.servers import http_server_base
try:
    from webkitpy.layout_tests.servers.basic_dns_server import DNSLogger, DNSServer, Resolver
except ImportError:
    print("Error importing DNSLogger, DNSServer, Resolver")

_log = logging.getLogger(__name__)


def doc_root(port_obj):
    doc_root = port_obj.get_option("wptserver_doc_root")
    if doc_root is None:
        return port_obj.host.filesystem.join("imported", "w3c", "web-platform-tests")
    return doc_root


def wpt_config_json(port_obj):
    fs = port_obj.host.filesystem
    config_wk_filepath = fs.join(port_obj.layout_tests_dir(), "imported", "w3c", "resources", "config.json")
    if not fs.isfile(config_wk_filepath):
        return
    config = json.loads(fs.read_text_file(config_wk_filepath))
    if port_obj.supports_localhost_aliases and not port_obj.get_option('disable_wpt_hostname_aliases'):
        config['server_host'] = '127.0.0.1'
        config['browser_host'] = 'web-platform.test'
        config['alternate_hosts'] = {'alt': 'not-web-platform.test'}
    config['ssl']['openssl']['base_path'] = fs.join(port_obj.results_directory(), "_wpt_certs")
    return config


def base_http_url(port_obj, localhost_only=False):
    config = wpt_config_json(port_obj)
    if not config:
        # This should only be hit by webkitpy unit tests
        _log.debug("No WPT config file found")
        return "http://localhost:8800/"
    ports = config["ports"]
    host = config["browser_host"] if not localhost_only else "localhost"
    return "http://" + host + ":" + str(ports["http"][0]) + "/"


def base_https_url(port_obj, localhost_only=False):
    config = wpt_config_json(port_obj)
    if not config:
        # This should only be hit by webkitpy unit tests
        _log.debug("No WPT config file found")
        return "https://localhost:9443/"
    ports = config["ports"]
    host = config["browser_host"] if not localhost_only else "localhost"
    return "https://" + host + ":" + str(ports["https"][0]) + "/"


def base_h2_url(port_obj, localhost_only=False):
    config = wpt_config_json(port_obj)
    if not config:
        # This should only be hit by webkitpy unit tests
        _log.debug("No WPT config file found")
        return "https://localhost:9000/"
    ports = config["ports"]
    host = config["browser_host"] if not localhost_only else "localhost"
    return "https://" + host + ":" + str(ports["h2"][0]) + "/"


def base_url_list(port_obj):
    config = wpt_config_json(port_obj)
    host = config["browser_host"]
    plain_port = str(config["ports"]["http"][0])
    tls_port = str(config["ports"]["https"][0])
    h2_port = str(config["ports"]["h2"][0])

    urls = [
        "http://{}:{}/".format(host, plain_port),
        "https://{}:{}/".format(host, tls_port),
        "https://{}:{}/".format(host, h2_port),
    ]
    # Some ports support aliases but this list is to be presented to users
    # so we include localhost which always will work in host browsers.
    if port_obj.supports_localhost_aliases and host not in ("127.0.0.1", "localhost"):
        urls += [
            "http://localhost:{}/".format(plain_port),
            "https://localhost:{}/".format(tls_port),
            "https://localhost:{}/".format(h2_port),
        ]

    return urls


def is_wpt_server_running(port_obj):
    config = wpt_config_json(port_obj)
    if not config:
        return False
    return http_server_base.HttpServerBase._is_running_on_port(config["ports"]["http"][0])


def suppress_dns_resolver_logs(log_string):
    return


def _aioquic_autoinstall_directory():
    # aioquic is provisioned through webkitpy's AutoInstall (see autoinstalled/aioquic.py), which unpacks
    # it into AutoInstall.directory and adds that directory to *this* process's sys.path only. The wpt
    # server runs in a separate subprocess with PYTHONPATH unset, so it would not see the package. Import
    # the autoinstalled module here to trigger provisioning, then return AutoInstall.directory so callers
    # can append it to the subprocess PYTHONPATH. Any failure (offline, missing wheel, autoinstall error)
    # must not take down non-webtransport WPT runs: swallow it and let the aioquic probe degrade gracefully.
    try:
        import webkitpy.autoinstalled.aioquic  # noqa: F401
        from webkitcorepy import AutoInstall
        return AutoInstall.directory
    except BaseException as e:
        _log.warning("aioquic autoinstall unavailable (%s); WebTransport-over-HTTP/3 server will be disabled.", e)
        return None


def _env_with_pythonpath_appended(directory):
    # Append (never prepend) so the autoinstall directory cannot shadow wpt's own vendored packages.
    env = os.environ.copy()
    if directory:
        existing = env.get('PYTHONPATH')
        env['PYTHONPATH'] = os.pathsep.join([existing, directory]) if existing else directory
    return env


class WebPlatformTestServer(http_server_base.HttpServerBase):
    def __init__(self, port_obj, name, pidfile=None):
        http_server_base.HttpServerBase.__init__(self, port_obj)
        self._output_dir = port_obj.results_directory()

        self._name = name
        self._log_file_name = '%s_process_log.out.txt' % (self._name)

        self._output_log_path = None
        self._wsout = None
        self._process = None
        self._dns_server = None

        port_has_local_dns_resolver = port_obj.port_name is not None and (port_obj.port_name == "mac" or "simulator" in port_obj.port_name)
        use_local_dns_resolver = port_obj.supports_localhost_aliases and port_has_local_dns_resolver and not port_obj.get_option('disable_wpt_hostname_aliases')

        if port_obj.supports_localhost_aliases:
            print("Port supports localhost aliases")
        else:
            print("Port does not support localhost aliases")
        if port_obj.get_option('disable_wpt_hostname_aliases'):
            print("Port has disabled hostname aliases")
        else:
            print("Port has enabled hostname aliases")
        if use_local_dns_resolver:
            print("Using local DNS resolver for", port_obj.port_name)
        else:
            print("Not Using local DNS resolver for", port_obj.port_name)

        if use_local_dns_resolver:
            logger = DNSLogger(logf=suppress_dns_resolver_logs)
            self._dns_server = DNSServer(Resolver(
                allowed_hosts=port_obj.localhost_aliases()), port=8053, address="127.0.0.1", logger=logger)

        self._pid_file = pidfile
        if not self._pid_file:
            self._pid_file = self._filesystem.join(self._runtime_path, '%s.pid' % self._name)

        self._filesystem = port_obj.host.filesystem
        self._layout_root = port_obj.layout_tests_dir()
        self._doc_root = self._filesystem.join(self._layout_root, doc_root(port_obj))

        self._doc_root_path = self._filesystem.join(self._layout_root, self._doc_root)
        self._config_filename = self._filesystem.join(self._doc_root_path, "config.json")

        # FIXME https://webkit.org/b/222703
        python_interp = sys.executable

        wpt_file = self._filesystem.join(self._doc_root_path, "wpt.py")
        self._start_cmd = [python_interp, wpt_file, "serve", "--config", self._config_filename]

        # The WebTransport-over-HTTP/3 server is opt-in upstream (tools/serve/serve.py) and depends on aioquic.
        # aioquic must live in the wpt subprocess's interpreter, not webkitpy's, so probe python_interp directly.
        # If it is missing, upstream's start_webtransport_h3_server catches the ImportError and sys.exit(0)s only
        # its own daemon child; the tracked main wpt process survives, so a fatal h3 liveness check would spin and
        # then abort the entire layout-test run. Degrade gracefully: enable h3 only when aioquic is importable.
        # aioquic is provisioned via AutoInstall, which lands it outside the subprocess's default sys.path, so its
        # directory must be injected into the subprocess PYTHONPATH for both the probe and the serve launch below.
        self._server_env = _env_with_pythonpath_appended(_aioquic_autoinstall_directory())
        # _aioquic_autoinstall_directory() provisions/verifies aioquic in webkitpy's interpreter; this probe
        # re-checks it in the serve interpreter (python_interp), which can differ, and is what gates --webtransport-h3.
        h3_available = self._executive.run_command([python_interp, "-c", "import aioquic"], env=self._server_env, return_exit_code=True) == 0
        if h3_available:
            self._start_cmd.append("--webtransport-h3")
        else:
            _log.warning("aioquic not available; WebTransport-over-HTTP/3 server disabled -- webtransport/ tests "
                         "will not run. See Phase 1b provisioning.")

        self._mappings = []
        config = wpt_config_json(port_obj)
        if config:
            ports = config["ports"]
            for key in ports:
                # Without aioquic the h3 server never starts, so it must not be part of the liveness/forward set.
                if key == "webtransport-h3" and not h3_available:
                    continue
                for value in ports[key]:
                    port = {"port": value}
                    if key == "https" or key == "wss":
                        port["sslcert"] = True
                    # webtransport-h3 is QUIC over UDP, not TCP; flag it so it is not forwarded as a TCP port.
                    if key == "webtransport-h3":
                        port["udp"] = True
                    self._mappings.append(port)

    def ports_to_forward(self):
        # Device port-forwarding is TCP-only; UDP ports (e.g. the QUIC/HTTP/3 WebTransport port) must not be
        # forwarded here, since forwarding them as TCP would silently break the h3 server on devices.
        return [mapping['port'] for mapping in self._mappings if not mapping.get('udp')]

    @staticmethod
    def _is_udp_port_listening(port):
        # QUIC/HTTP/3 is UDP, so the inherited TCP connect() liveness check never sees it. A dead h3 server
        # otherwise surfaces as mass connect timeouts in the tests, which is exactly what we want to catch
        # here instead. The running server holds the UDP port bound, so a bind attempt fails with EADDRINUSE.
        # Bind to 127.0.0.1 (the address the h3 server binds) rather than 'localhost' to avoid a false negative
        # from an IPv6 localhost resolution. Match EADDRINUSE only: EACCES means the probe itself was denied
        # (sandbox/firewall), not that the server is up, so it must not be read as "running".
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.bind(('127.0.0.1', port))
        except OSError as e:
            if e.errno == errno.EADDRINUSE:
                return True
            raise
        finally:
            s.close()
        return False

    def _is_server_running_on_all_ports(self):
        if not self._port_obj.host.platform.is_win() and not self._executive.check_running_pid(self._pid):
            _log.debug("Server isn't running at all")
            raise http_server_base.ServerError("Server exited")

        for mapping in self._mappings:
            if mapping.get('udp'):
                if not self._is_udp_port_listening(mapping['port']):
                    _log.debug("UDP server NOT running on %d" % mapping['port'])
                    return False
                _log.debug("UDP server running on %d" % mapping['port'])
            elif not self._is_running_on_port(mapping['port']):
                return False
        return True

    def first_port(self, port_obj):
        config = wpt_config_json(port_obj)
        if not config:
            return None
        return config["ports"]["http"][0]

    def _prepare_config(self):
        self._filesystem.maybe_make_directory(self._output_dir)
        self._output_log_path = self._filesystem.join(self._output_dir, self._log_file_name)
        self._wsout = self._filesystem.open_text_file_for_writing(self._output_log_path)

        _log.debug('Copying WebKit web platform server config.json')
        config = wpt_config_json(self._port_obj)
        if config:
            self._filesystem.write_text_file(self._config_filename, json.dumps(config))

    def _spawn_process(self):
        self._process = self._executive.popen(self._start_cmd, cwd=self._doc_root_path, env=self._server_env, shell=False, stdin=self._executive.PIPE, stdout=self._wsout, stderr=self._wsout)
        self._filesystem.write_text_file(self._pid_file, str(self._process.pid))

        if self._dns_server:
            self._dns_server.start_thread()

        # Wait a second for the server to actually start so that tests do not start until server is running.
        time.sleep(1)

        # The server is not running after 1 second, something went wrong.
        if self._process.poll() is not None:
            self._stop_running_server()
            error_log = ('WPT Server process exited prematurely with status code %s\n' % self._process.returncode
                         + 'The cmdline for running the WPT server was: %s\n' % self._start_cmd
                         + 'The working dir was: %s\n' % self._doc_root_path)
            if self._output_log_path is not None and self._filesystem.exists(self._output_log_path):
                error_log += 'Check the logfile for the command at: %s\n' % self._output_log_path
            raise http_server_base.ServerError(error_log)

        return self._process.pid

    def _stop_running_server(self):
        _log.debug('Cleaning WPT web platform server config.json')
        if self._filesystem.isfile(self._config_filename):
            self._filesystem.remove(self._config_filename)

        if self._wsout:
            self._wsout.close()
            self._wsout = None

        if self._dns_server and hasattr(self._dns_server, "thread") and self._dns_server.isAlive():
            self._dns_server.stop()

        if self._process is not None:
            self._process.poll()

        if self._pid:
            # kill_process will not kill the subprocesses, interrupt does the job.
            self._executive.interrupt(self._pid)

        self._remove_pid_file()
