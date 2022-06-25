#!/usr/bin/env python3

import argparse
import logging
import os
import sys

# Since we execute this script directly as a subprocess, we need to ensure
# that Tools/Scripts is in sys.path for the next imports to work correctly.
script_dir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../..'))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from webkitpy.autoinstalled import twisted

from twisted.web import static, server
from twisted.web.resource import Resource
from twisted.internet import reactor

_log = logging.getLogger(__name__)


class ServerControl(Resource):
    isLeaf = True

    def render_GET(self, request):
        _log.info("Serving request %s" % request)
        reactor.stop()
        return ""

    def render_POST(self, request):
        _log.info("Serving request %s" % request)
        sys.stdout.write(request.content.read().decode('utf-8'))
        sys.stdout.flush()
        reactor.stop()
        return 'OK'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='python twisted_http_server.py web_root')
    parser.add_argument('web_root')
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--interface', default='')
    parser.add_argument('--log-path', default='/tmp/run-benchmark-http.log')
    args = parser.parse_args()
    web_root = static.File(args.web_root)
    serverControl = ServerControl()
    web_root.putChild('shutdown'.encode('utf-8'), serverControl)
    web_root.putChild('report'.encode('utf-8'), serverControl)
    reactor.listenTCP(args.port, server.Site(web_root, logPath=args.log_path), interface=args.interface)
    reactor.run()
