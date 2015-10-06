#!/usr/bin/python

import json
import os
import subprocess
import tempfile


def main():
    with open(abspath_from_root('config.json')) as config_file:
        config = json.load(config_file)
        cache_dir = abspath_from_root(config['cacheDirectory'])
        httpd_config_file = abspath_from_root(config['remoteServer']['httpdConfig'])
        httpd_pid_file = abspath_from_root(config['remoteServer']['httpdPID'])
        httpd_error_log_file = abspath_from_root(config['remoteServer']['httpdErroLog'])
        doc_root = abspath_from_root('public')

        if not os.path.isdir(cache_dir):
            os.makedirs(cache_dir)
        os.chmod(cache_dir, 0755)

        httpd_mutax_dir = tempfile.mkdtemp()
        try:
            subprocess.call(['httpd',
                '-f', httpd_config_file,
                '-c', 'PidFile ' + httpd_pid_file,
                '-c', 'Mutex file:' + httpd_mutax_dir,
                '-c', 'DocumentRoot ' + doc_root,
                '-c', 'ErrorLog ' + httpd_error_log_file,
                '-X'])
        finally:
            os.rmdir(httpd_mutax_dir)


def abspath_from_root(relpath):
    return os.path.abspath(os.path.join(os.path.dirname(__file__), '../', relpath))


if __name__ == "__main__":
    main()
