#!/usr/bin/env python

import logging
import os
import re
import socket
import subprocess
import time

from http_server_driver import HTTPServerDriver


_log = logging.getLogger(__name__)


class SimpleHTTPServerDriver(HTTPServerDriver):

    """This class depends on unix environment, need to be modified to achieve crossplatform compability
    """

    def __init__(self):
        self.serverProcess = None
        self.serverPort = 0
        # FIXME: This may not be reliable.
        _log.info('Finding the IP address of current machine')
        try:
            self.ip = [ip for ip in socket.gethostbyname_ex(socket.gethostname())[2] if not ip.startswith("127.")][0]
            _log.info('IP of current machine is: %s' % self.ip)
        except:
            _log.error('Cannot get the ip address of current machine')
            raise

    def serve(self, webroot):
        oldWorkingDirectory = os.getcwd()
        os.chdir(os.path.dirname(os.path.abspath(__file__)))
        _log.info('Lauchning an http server')
        self.serverProcess = subprocess.Popen(['/usr/bin/python', 'http_server/twisted_http_server.py', webroot], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        os.chdir(oldWorkingDirectory)
        maxAttempt = 5
        interval = 0.5
        _log.info('Start to fetching the port number of the http server')
        try:
            import psutil
            for attempt in xrange(maxAttempt):
                try:
                    self.serverPort = psutil.Process(self.serverProcess.pid).connections()[0][3][1]
                    if self.serverPort:
                        _log.info('HTTP Server is serving at port: %d', self.serverPort)
                        break
                except IndexError:
                    pass
                _log.info('Server port is not found this time, retry after %f seconds' % interval)
                time.sleep(interval)
                interval *= 2
        except ImportError:
            try:
                for attempt in xrange(maxAttempt):
                    try:
                        p = subprocess.Popen(' '.join(['/usr/sbin/lsof', '-a', '-iTCP', '-sTCP:LISTEN', '-p', str(self.serverProcess.pid)]), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                        self.serverPort = int(re.findall('TCP \*:(\d+) \(LISTEN\)', p.communicate()[0])[0])
                        if self.serverPort:
                            _log.info('HTTP Server is serving at port: %d', self.serverPort)
                            break
                    # Raising exception means the server is not ready to server, try later
                    except ValueError:
                        pass
                    except IndexError:
                        pass
                    _log.info('Server port is not found this time, retry after %f seconds' % interval)
                    time.sleep(interval)
                    interval *= 2
            except:
                raise Exception("Server may not be serving")

    def baseUrl(self):
        return "http://%s:%d" % (self.ip, self.serverPort)

    def fetchResult(self):
        return self.serverProcess.communicate()[0]
