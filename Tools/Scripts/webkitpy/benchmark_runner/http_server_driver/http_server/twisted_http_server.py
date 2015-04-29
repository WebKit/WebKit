#!/usr/bin/env python

from twisted.web import static, server
from twisted.web.resource import Resource
from twisted.internet import reactor
import argparse
import sys


class ServerControl(Resource):
    isLeaf = True

    def render_GET(self, request):
        reactor.stop()
        return ""

    def render_POST(self, request):
        sys.stdout.write(request.content.getvalue())
        sys.stdout.flush()
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
