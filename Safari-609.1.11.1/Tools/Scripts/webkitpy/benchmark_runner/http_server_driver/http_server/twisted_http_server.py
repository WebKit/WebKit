#!/usr/bin/env python

import argparse
import logging
import os
import sys
from pkg_resources import require, VersionConflict, DistributionNotFound

try:
    require("Twisted>=15.5.0")
    import twisted
except (ImportError, VersionConflict, DistributionNotFound):
    sys.path.append(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../..')))
    from webkitpy.thirdparty.autoinstalled.twisted_15_5_0 import twisted

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
        sys.stdout.write(request.content.read())
        sys.stdout.flush()
        reactor.stop()
        return 'OK'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='python twisted_http_server.py web_root')
    parser.add_argument('web_root')
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--interface', default='')
    args = parser.parse_args()
    web_root = static.File(args.web_root)
    serverControl = ServerControl()
    web_root.putChild('shutdown', serverControl)
    web_root.putChild('report', serverControl)
    reactor.listenTCP(args.port, server.Site(web_root), interface=args.interface)
    reactor.run()
