# This Source Code Form is subject to the terms of the 3-clause BSD License
# http://www.w3.org/Consortium/Legal/2008/03-bsd-license.html

import imp
import sys
import logging
import optparse
import os

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

sys.path.insert(1, ".")
import serve as WebPlatformTestServer

# This script is used to launch the web platform test server main script (serve.py) and stop it when asked by run-webkit-tests


def main(argv, stdout, stderr):
    # This is a copy of serve.py main function, except for the wait step
    config = WebPlatformTestServer.load_config("config.default.json", "config.json")
    WebPlatformTestServer.setup_logger(config["log_level"])
    ssl_env = WebPlatformTestServer.get_ssl_environment(config)
    with WebPlatformTestServer.get_ssl_environment(config) as ssl_env:
        config_, started_servers = WebPlatformTestServer.start(config, ssl_env)

        for protocol, servers in started_servers.items():
            for port, process in servers:
                logger.info("%s, port:%d, pid:%d" % (protocol, port, process.proc.pid))

    sys.stdin.read(1)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:], sys.stdout, sys.stderr))
