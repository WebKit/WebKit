# This Source Code Form is subject to the terms of the 3-clause BSD License
# http://www.w3.org/Consortium/Legal/2008/03-bsd-license.html

import json
import logging
import os
import sys
import uuid

logger = logging.getLogger('web-platform-test-launcher')
logging.basicConfig(level=logging.DEBUG)

try:
    sys.path.insert(0, os.getcwd())
    import tools.serve.serve as WebPlatformTestServer
    sys.path.insert(0, os.path.join(os.getcwd(), "tools", "wptserve", "wptserve"))
    import stash
except ImportError, e:
    logger.critical("Import of wpt serve module failed.\n"
        "Please check that the file serve.py is present in the web-platform-tests folder.\n"
        "Please also check that __init__.py files in the web-platform-tests/tools folder and subfolders are also present.")
    raise

# This script is used to launch the web platform test server main script (serve.py) and stop it when asked by run-webkit-tests


def main(argv, stdout, stderr):
    # This is a copy of serve.py main function, except for the wait step
    config = WebPlatformTestServer.load_config("config.default.json", "config.json")
    WebPlatformTestServer.setup_logger(config["log_level"])
    logged_servers = []

    with stash.StashServer((config["host"], WebPlatformTestServer.get_port()), authkey=str(uuid.uuid4())):
        with WebPlatformTestServer.get_ssl_environment(config) as ssl_env:
            config_, started_servers = WebPlatformTestServer.start(config, ssl_env, WebPlatformTestServer.default_routes())

            for protocol, servers in started_servers.items():
                for port, process in servers:
                    logged_servers.append({"protocol": protocol, "port": port, "pid": process.proc.pid})
                    logger.info("%s, port:%d, pid:%d" % (protocol, port, process.proc.pid))

            # Write pids in a file in case abrupt shutdown is needed
            with open(argv[0], "wb") as servers_file:
                json.dump(logged_servers, servers_file)

            sys.stdin.read(1)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:], sys.stdout, sys.stderr))
