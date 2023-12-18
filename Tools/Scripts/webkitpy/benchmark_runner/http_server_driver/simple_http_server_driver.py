import logging
import os
import re
import subprocess
import shutil
import sys
import time

from webkitpy.benchmark_runner.http_server_driver.http_server_driver import HTTPServerDriver


_log = logging.getLogger(__name__)


class SimpleHTTPServerDriver(HTTPServerDriver):

    """This class depends on unix environment, need to be modified to achieve crossplatform compability
    """

    platforms = ['osx', 'linux']

    def __init__(self, **kwargs):
        self._server_process = None
        self._server_port = 0
        self._ip = '127.0.0.1'
        self._http_log_path = None
        self._server_type = kwargs.get('server_type', 'twisted')
        self.set_device_id(kwargs.get('device_id'))
        self._ensure_http_server_dependencies()

    def serve(self, web_root):
        _log.info('Launching an http server')
        http_server_file = 'http_server/{}_http_server.py'.format(self._server_type)
        http_server_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), http_server_file)
        extra_args = []
        if self._ip:
            extra_args.extend(['--interface', self._ip])
        if self._http_log_path:
            extra_args.extend(['--log-path', self._http_log_path])
            _log.info('HTTP requests will be logged to {}'.format(self._http_log_path))
        self._server_port = 0
        self._server_process = subprocess.Popen([sys.executable, http_server_path, web_root] + extra_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        max_attempt = 7
        retry_sequence = map(lambda attempt: attempt != max_attempt - 1, range(max_attempt))
        interval = 0.5
        _log.info('Start to fetching the port number of the http server')
        for retry in retry_sequence:
            self._find_http_server_port()
            if self._server_port:
                _log.info('HTTP Server is serving at port: %d', self._server_port)
                break
            assert self._server_process.poll() is None, 'HTTP Server Process is not running'
            if not retry:
                continue
            _log.info('Server port is not found this time, retry after {} seconds'.format(interval))
            time.sleep(interval)
            interval *= 2
        else:
            raise Exception("Server is not listening on port, max tries exceeded. HTTP server may be installing dependent modules.")
        self._wait_for_http_server()

    def _find_http_server_port(self):
        if self._server_process.poll() is not None:
            stdout_data, stderr_data = self._server_process.communicate()
            raise RuntimeError('The http server terminated unexpectedly with return code {} and with the following output:\n{}'.format(self._server_process.returncode, stdout_data + stderr_data))
        try:
            import psutil
            connections = psutil.Process(self._server_process.pid).connections()
            if connections and connections[0].laddr and connections[0].laddr[1] and connections[0].status == 'LISTEN':
                self._server_port = connections[0].laddr[1]
        except ImportError:
            try:
                # lsof on Linux is shipped on /usr/bin typically, but on Mac on /usr/sbin
                lsof_path = shutil.which('lsof') or '/usr/sbin/lsof'
                output = subprocess.check_output([lsof_path, '-a', '-P', '-iTCP', '-sTCP:LISTEN', '-p', str(self._server_process.pid)])
                self._server_port = int(re.search(r'TCP .*:(\d+) \(LISTEN\)', str(output)).group(1))
            except Exception as error:
                _log.info('Error: %s' % error)

    def _wait_for_http_server(self):
        max_attempt = 5
        # Wait for server to be up completely before exiting
        for attempt in range(max_attempt):
            try:
                subprocess.check_call(["curl", "--silent", "--head", "--fail", "--output", "/dev/null", self.base_url()])
                return
            except Exception as error:
                _log.info('Server not running yet: %s' % error)
                time.sleep(1)
        raise Exception('Server not running, max tries exceeded: %s' % error)

    def base_url(self):
        return "http://%s:%d" % (self._ip, self._server_port)

    def fetch_result(self):
        (stdout, stderr) = self._server_process.communicate()
        print(stderr)
        return stdout

    def kill_server(self):
        try:
            self._server_port = 0
            if not self._server_process:
                return
            if self._server_process.poll() is None:
                self._server_process.terminate()
        except OSError as error:
            _log.info('Error terminating server process: %s' % (error))

    def get_return_code(self):
        return self._server_process.returncode

    def set_device_id(self, device_id):
        pass

    def set_http_log(self, log_path):
        self._http_log_path = log_path

    def set_http_server_type(self, server_type):
        self._server_type = server_type

    def _ensure_http_server_dependencies(self):
        _log.info('Ensure dependencies of http server is satisfied')
        if(self._server_type == 'twisted'):
            from webkitpy.autoinstalled import twisted
