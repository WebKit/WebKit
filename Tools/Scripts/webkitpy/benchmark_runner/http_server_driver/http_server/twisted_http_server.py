#!/usr/bin/env python

import argparse
import logging
import sys

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
    parser = argparse.ArgumentParser(description='python TwistedHTTPServer.py webRoot')
    parser.add_argument('webRoot')
    args = parser.parse_args()
    webRoot = static.File(args.webRoot)
    serverControl = ServerControl()
    webRoot.putChild('shutdown', serverControl)
    webRoot.putChild('report', serverControl)
    reactor.listenTCP(0, server.Site(webRoot))
    reactor.run()
