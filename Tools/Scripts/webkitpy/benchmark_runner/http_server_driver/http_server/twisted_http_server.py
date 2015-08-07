#!/usr/bin/env python

import argparse
import logging
import os
import sys

try:
    import twisted
except ImportError:
    sys.path.append(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../..')))
    from webkitpy.thirdparty.autoinstalled.twisted import twisted

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
        sys.stdout.write(request.content.getvalue())
        sys.stdout.flush()
        reactor.stop()
        return 'OK'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='python twisted_http_server.py web_root')
    parser.add_argument('web_root')
    parser.add_argument('--port', type=int, default=0)
    args = parser.parse_args()
    web_root = static.File(args.web_root)
    serverControl = ServerControl()
    web_root.putChild('shutdown', serverControl)
    web_root.putChild('report', serverControl)
    reactor.listenTCP(args.port, server.Site(web_root))
    reactor.run()
