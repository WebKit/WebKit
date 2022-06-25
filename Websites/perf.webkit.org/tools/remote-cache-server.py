#!/usr/bin/python

import json
import os
import shutil
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) <= 1:
        sys.exit('Specify the command: start, stop, reset')

    command = sys.argv[1]

    with open(abspath_from_root('config.json')) as config_file:
        config = json.load(config_file)

    cache_dir = abspath_from_root(config['cacheDirectory'])
    if command == 'start':
        if not os.path.isdir(cache_dir):
            os.makedirs(cache_dir)
        os.chmod(cache_dir, 0755)

        start_httpd(config['remoteServer'])
    elif command == 'stop':
        stop_httpd(config['remoteServer'])
    elif command == 'reset':
        shutil.rmtree(cache_dir)
    else:
        sys.exit('Unknown command: ' + command)


def start_httpd(remote_server_config):
    httpd_config_file = abspath_from_root(remote_server_config['httpdConfig'])
    httpd_pid_file = abspath_from_root(remote_server_config['httpdPID'])
    httpd_error_log_file = abspath_from_root(remote_server_config['httpdErrorLog'])
    httpd_mutex_dir = abspath_from_root(remote_server_config['httpdMutexDir'])
    version_info = subprocess.check_output(['php', '-v'], stderr=subprocess.PIPE)
    php_version = 'PHP5' if 'PHP 5' in version_info else 'PHP7'

    if not os.path.isdir(httpd_mutex_dir):
        os.makedirs(httpd_mutex_dir)
    os.chmod(httpd_mutex_dir, 0755)

    doc_root = abspath_from_root('public')

    # don't use -X since http://svn.apache.org/viewvc?view=revision&revision=1711479
    subprocess.call(['httpd',
        '-f', httpd_config_file,
        '-c', 'PidFile ' + httpd_pid_file,
        '-c', 'Mutex file:' + httpd_mutex_dir,
        '-c', 'DocumentRoot ' + doc_root,
        '-c', 'ErrorLog ' + httpd_error_log_file,
        '-D', php_version])

def stop_httpd(remote_server_config):
    httpd_pid_file = abspath_from_root(remote_server_config['httpdPID'])
    if not os.path.isfile(httpd_pid_file):
        sys.exit("PID file doesn't exist at %s" % httpd_pid_file)

    with open(httpd_pid_file) as pid_file:
        pid = pid_file.read().strip()
        print "Stopping", pid
        subprocess.call(['kill', '-TERM', pid])


def abspath_from_root(relpath):
    return os.path.abspath(os.path.join(os.path.dirname(__file__), '../', relpath))


if __name__ == "__main__":
    main()
