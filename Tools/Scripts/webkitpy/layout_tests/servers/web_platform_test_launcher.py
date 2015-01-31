# This Source Code Form is subject to the terms of the 3-clause BSD License
# http://www.w3.org/Consortium/Legal/2008/03-bsd-license.html

import imp
import logging
import os
import sys

logger = logging.getLogger('web-platform-test-launcher')
logging.basicConfig(level=logging.DEBUG)


def create_wpt_empty_file_if_needed(relative_path):
    file_path = os.path.join(os.getcwd(), os.path.sep.join(relative_path))
    if not os.path.isfile(file_path):
        logger.warning(file_path + ' is not present, creating it as empty file')
        open(file_path, 'a').close()

create_wpt_empty_file_if_needed(['tools', '__init__.py'])
create_wpt_empty_file_if_needed(['tools', 'scripts', '__init__.py'])
create_wpt_empty_file_if_needed(['tools', 'pywebsocket', 'src', 'test', '__init__.py'])
create_wpt_empty_file_if_needed(['tools', 'webdriver', 'webdriver', '__init__.py'])
create_wpt_empty_file_if_needed(['tools', 'html5lib', 'html5lib', 'treeadapters', '__init__.py'])
create_wpt_empty_file_if_needed(['tools', 'html5lib', 'html5lib', 'filters', '__init__.py'])

try:
    sys.path.insert(0, os.getcwd())
    import serve as WebPlatformTestServer
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
    ssl_env = WebPlatformTestServer.get_ssl_environment(config)
    with WebPlatformTestServer.get_ssl_environment(config) as ssl_env:
        config_, started_servers = WebPlatformTestServer.start(config, ssl_env)

        for protocol, servers in started_servers.items():
            for port, process in servers:
                logger.info("%s, port:%d, pid:%d" % (protocol, port, process.proc.pid))

    sys.stdin.read(1)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:], sys.stdout, sys.stderr))
