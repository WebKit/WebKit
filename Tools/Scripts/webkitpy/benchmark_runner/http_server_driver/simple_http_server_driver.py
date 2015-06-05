#!/usr/bin/env python

import logging
import os
import re
import socket
import subprocess
import sys
import time

from http_server_driver import HTTPServerDriver


_log = logging.getLogger(__name__)


class SimpleHTTPServerDriver(HTTPServerDriver):

    """This class depends on unix environment, need to be modified to achieve crossplatform compability
    """

    def __init__(self):
        self.server_process = None
        self.server_port = 0
        # FIXME: This may not be reliable.
        _log.info('Finding the IP address of current machine')
        try:
            self.ip = [ip for ip in socket.gethostbyname_ex(socket.gethostname())[2] if not ip.startswith("127.")][0]
            _log.info('IP of current machine is: %s' % self.ip)
        except Exception as error:
            _log.error('Cannot get the ip address of current machine - Error: %s' % error)
            raise

    def serve(self, web_root):
        _log.info('Launching an http server')
        http_server_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "http_server/twisted_http_server.py")
        self.server_process = subprocess.Popen(["/usr/bin/python", http_server_path, web_root], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        maxAttempt = 5
        interval = 0.5
        _log.info('Start to fetching the port number of the http server')
        try:
            import psutil
            for attempt in xrange(maxAttempt):
                try:
                    self.server_port = psutil.Process(self.server_process.pid).connections()[0][3][1]
                    if self.server_port:
                        _log.info('HTTP Server is serving at port: %d', self.server_port)
                        break
                except IndexError:
                    pass
                _log.info('Server port is not found this time, retry after %f seconds' % interval)
                time.sleep(interval)
                interval *= 2
            else:
                raise Exception("Cannot listen to server, max tries exceeded")
        except ImportError:
            for attempt in xrange(maxAttempt):
                try:
                    output = subprocess.check_output(['/usr/sbin/lsof', '-a', '-iTCP', '-sTCP:LISTEN', '-p', str(self.server_process.pid)])
                    self.server_port = int(re.search('TCP \*:(\d+) \(LISTEN\)', output).group(1))
                    if self.server_port:
                        _log.info('HTTP Server is serving at port: %d', self.server_port)
                        break
                except Exception as error:
                     _log.info('Error: %s' % error)
                _log.info('Server port is not found this time, retry after %f seconds' % interval)
                time.sleep(interval)
                interval *= 2
            else:
                raise Exception("Cannot listen to server, max tries exceeded")

        # Wait for server to be up completely before exiting
        for attempt in xrange(maxAttempt):
            try:
                subprocess.check_call(["curl", "--silent", "--head", "--fail", "--output", "/dev/null", self.baseUrl()])
                return
            except Exception as error:
                _log.info('Server not running yet: %s' % error)
                time.sleep(interval)
        raise Exception('Server not running, max tries exceeded: %s' % error)


    def baseUrl(self):
        return "http://%s:%d" % (self.ip, self.server_port)

    def fetchResult(self):
        (stdout, stderr) = self.server_process.communicate()
        print stderr
        return stdout

    def killServer(self):
        try:
            self.server_process.terminate()
        except OSError as error:
            _log.info('Error terminating server process: %s' % (error))

    def getReturnCode(self):
        return self.server_process.returncode
