#!/usr/bin/env python
#
# Copyright (C) 2017 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
import sys
import signal
import os
import argparse
import subprocess
import tempfile
import shutil
import socket
import json
import traceback
import multiprocessing
from time import sleep

test_buildbot_master_tac = """
import os
from twisted.application import service
try:
    from buildbot.master.bot import BuildMaster
except:
    from buildbot.master import BuildMaster

basedir = os.path.dirname(os.path.realpath(__file__))
configfile = r'master.cfg'

application = service.Application('buildmaster')
BuildMaster(basedir, configfile).setServiceParent(application)
"""

worker_buildbot_master_tac = """
import os
from twisted.application import service
from buildslave.bot import BuildSlave

basedir = os.path.dirname(os.path.realpath(__file__))
buildmaster_host = 'localhost'
port = 17000
slavename = '%(worker)s'
passwd = '1234'
keepalive = 600
usepty = 1

application = service.Application('buildslave')
BuildSlave(buildmaster_host, port, slavename, passwd, basedir, keepalive, usepty).setServiceParent(application)
"""


def check_tcp_port_open(address, port):
    s = socket.socket()
    try:
        s.connect((address, port))
        return True
    except:
        return False


def upgrade_db_needed(log):
    try:
        with open(log) as f:
            for l in f:
                if 'upgrade the database' in l:
                    return True
    except:
        return False
    return False


def create_tempdir(tmpdir=None):
    if tmpdir is not None:
        if not os.path.isdir(tmpdir):
            raise ValueError('%s is not a directory' % tmpdir)
        return tempfile.mkdtemp(prefix=os.path.join(os.path.abspath(tmpdir), 'tmp'))
    return tempfile.mkdtemp()


def print_if_error_stdout_stderr(cmd, retcode, stdout=None, stderr=None, extramsg=None):
    if retcode != 0:
        if type(cmd) == type([]):
            cmd = ' '.join(cmd)
        print('WARNING: "%s" returned %s status code' % (cmd, retcode))
        if stdout is not None:
            print(stdout)
        if stderr is not None:
            print(stderr)
        if extramsg is not None:
            print(extramsg)


def setup_master_workdir(configdir, base_workdir):
    master_workdir = os.path.join(base_workdir, 'master')
    print('Copying files from %s to %s ...' % (configdir, master_workdir))
    shutil.copytree(configdir, master_workdir)
    print('Generating buildbot files at %s ...' % master_workdir)
    with open(os.path.join(master_workdir, 'buildbot.tac'), 'w') as f:
        f.write(test_buildbot_master_tac)
    mkpwd_cmd = ['./make_passwords_json.py']
    mkpwd_process = subprocess.Popen(mkpwd_cmd, cwd=master_workdir,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout, stderr = mkpwd_process.communicate()
    print_if_error_stdout_stderr(mkpwd_cmd, mkpwd_process.returncode, stdout, stderr)
    return master_workdir


def wait_for_master_ready(master_workdir):
    master_ready_check_counter = 0
    while True:
        if os.path.isfile(os.path.join(master_workdir, '.master-is-ready')):
            return
        if master_ready_check_counter > 60:
            raise RuntimeError('ERROR: Aborting after waiting 60 seconds for the master to start.')
        sleep(1)
        master_ready_check_counter += 1


def start_master(master_workdir):
    # This is started via multiprocessing. We set a new process group here
    # to be able to reliably kill this subprocess and all of its child on clean.
    os.setsid()
    buildmasterlog = os.path.join(master_workdir, 'buildmaster.log')
    dbupgraded = False
    retry = True
    if check_tcp_port_open('localhost', 8710):
        print('ERROR: There is some process already listening in port 8170')
        return 1
    while retry:
        retry = False
        print('Starting the twistd process ...')
        twistd_cmd = ['twistd', '-l', buildmasterlog, '-noy', 'buildbot.tac']
        twistd_process = subprocess.Popen(twistd_cmd, cwd=master_workdir,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        while twistd_process.poll() is None:
            if check_tcp_port_open('localhost', 8710):
                print('Test buildmaster ready!.\n\n'
                     + ' - See buildmaster log:\n'
                     + '     tail -f %s\n' % buildmasterlog
                     + ' - Open a browser to:\n'
                     + '     http://localhost:8710\n'
                     + ' - Credentials for triggering manual builds:\n'
                     + '     login:     committer@webkit.org\n'
                     + '     password:  committerpassword\n')
                with open(os.path.join(master_workdir, '.master-is-ready'), 'w') as f:
                    f.write('ready')
                twistd_process.wait()
                return 0
            sleep(1)
        stdout, stderr = twistd_process.communicate()
        if twistd_process.returncode == 0 and upgrade_db_needed(buildmasterlog) and not dbupgraded:
            retry = True
            dbupgraded = True
            print('Upgrading the database ...')
            upgrade_cmd = ['buildbot', 'upgrade-master', master_workdir]
            upgrade_process = subprocess.Popen(upgrade_cmd, cwd=master_workdir,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            stdout, stderr = upgrade_process.communicate()
            print_if_error_stdout_stderr(upgrade_cmd, upgrade_process.returncode, stdout, stderr)
        else:
            print_if_error_stdout_stderr(twistd_cmd, twistd_process.returncode, stdout, stderr,
                                         'Check the log at %s' % buildmasterlog)
    return 0


def get_list_workers(master_workdir):
    password_list = os.path.join(master_workdir, 'passwords.json')
    with open(password_list) as f:
        passwords = json.load(f)
    list_workers = []
    for worker in passwords.keys():
        list_workers.append(str(worker))
    return list_workers


def start_worker(base_workdir, worker):
    # This is started via multiprocessing. We set a new process group here
    # to be able to reliably kill this subprocess and all of its child on clean.
    os.setsid()
    worker_workdir = os.path.join(base_workdir, worker)
    os.mkdir(worker_workdir)
    with open(os.path.join(worker_workdir, 'buildbot.tac'), 'w') as f:
        f.write(worker_buildbot_master_tac % {'worker': worker})
    twistd_cmd = ['twistd', '-l', 'worker.log', '-noy', 'buildbot.tac']
    twistd_worker_process = subprocess.Popen(twistd_cmd, cwd=worker_workdir,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        stdout, stderr = twistd_worker_process.communicate()
    except:
        twistd_worker_process.kill()
        return
    print_if_error_stdout_stderr(twistd_cmd, twistd_worker_process.returncode, stdout, stderr,
                                 'Check the log at %s' % os.path.join(worker_workdir, 'worker.log'))


def clean(temp_dir):
    if os.path.isdir(temp_dir):
        print('\n\nCleaning %s ... \n' % (temp_dir))
        # shutil.rmtree can fail if we hold an open file descriptor on temp_dir
        # (which is very likely when cleaning) or if temp_dir is a NFS mount.
        # Use rm instead that always works.
        rm = subprocess.Popen(['rm', '-fr', temp_dir])
        rm.wait()


def cmd_exists(cmd):
    return any(os.access(os.path.join(path, cmd), os.X_OK)
               for path in os.environ['PATH'].split(os.pathsep))


def check_buildbot_installed():
    if cmd_exists('twistd') and cmd_exists('buildbot'):
        return
    raise RuntimeError('Buildbot is not installed.')


def setup_virtualenv(base_workdir_temp):
    if cmd_exists('virtualenv'):
        print('Setting up virtualenv at %s ... ' % base_workdir_temp)
        virtualenv_cmd = ['virtualenv', '-p', 'python2', 'venv']
        virtualenv_process = subprocess.Popen(virtualenv_cmd, cwd=base_workdir_temp,
                                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = virtualenv_process.communicate()
        print_if_error_stdout_stderr(virtualenv_cmd, virtualenv_process.returncode, stdout, stderr)
        virtualenv_bindir = os.path.join(base_workdir_temp, 'venv', 'bin')
        virtualenv_pip = os.path.join(virtualenv_bindir, 'pip')
        if not os.access(virtualenv_pip, os.X_OK):
            print('Something went wrong setting up virtualenv'
                  'Trying to continue using the system version of buildbot')
            return
        print('Setting up buildbot dependencies on the virtualenv ... ')
        # The idea is to install the very same version of buildbot and its
        # dependencies than the ones used for running https://build.webkit.org/about
        pip_cmd = [virtualenv_pip, 'install',
                    'buildbot==0.8.6p1',
                    'buildbot-slave==0.8.6p1',
                    'twisted==12.1.0',
                    'jinja2==2.6',
                    'sqlalchemy==0.7.8',
                    'sqlalchemy-migrate==0.7.2']
        pip_process = subprocess.Popen(pip_cmd, cwd=base_workdir_temp,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = pip_process.communicate()
        print_if_error_stdout_stderr(pip_cmd, pip_process.returncode, stdout, stderr)
        os.environ['PATH'] = virtualenv_bindir + ':' + os.environ['PATH']
        return
    print('WARNING: virtualenv not installed. '
          'Trying to continue using the system version of buildbot')


def configdir_is_valid(configdir):
    return(os.path.isdir(configdir) and
           os.path.isfile(os.path.join(configdir, 'config.json')) and
           os.path.isfile(os.path.join(configdir, 'master.cfg')) and
           os.access(os.path.join(configdir, 'make_passwords_json.py'), os.X_OK))


def main(configdir, basetempdir=None, no_clean=False, no_workers=False, use_system_version=False):
    configdir = os.path.abspath(os.path.realpath(configdir))
    if not configdir_is_valid(configdir):
        raise ValueError('The configdir %s dont contains the buildmaster files expected by this script' % configdir)
    base_workdir_temp = os.path.abspath(os.path.realpath(create_tempdir(basetempdir)))
    if base_workdir_temp.startswith(configdir):
        raise ValueError('The temporal working directory %s cant be located inside configdir %s' % (base_workdir_temp, configdir))
    try:
        if not use_system_version:
            setup_virtualenv(base_workdir_temp)
        check_buildbot_installed()
        master_workdir = setup_master_workdir(configdir, base_workdir_temp)
        master_runner = multiprocessing.Process(target=start_master, args=(master_workdir,))
        master_runner.start()
        wait_for_master_ready(master_workdir)
        if no_workers:
            print(' - To manually attach a build worker use this info:\n'
                 + '     TCP port for the worker-to-master connection: 17000\n'
                 + '     worker-id: the one defined at %s\n' % os.path.join(master_workdir, 'passwords.json')
                 + '     password:  1234\n')
        else:
            worker_runners = []
            for worker in get_list_workers(master_workdir):
                worker_runner = multiprocessing.Process(target=start_worker, args=(base_workdir_temp, worker,))
                worker_runner.start()
                worker_runners.append(worker_runner)
            print(' - Workers started!.\n'
                 + '     Check the log for each one at %s/${worker-name-id}/worker.log\n' % base_workdir_temp
                 + '     tail -f %s/*/worker.log\n' % base_workdir_temp)
            for worker_runner in worker_runners:
                worker_runner.join()
        master_runner.join()
    except:
        traceback.print_exc()
    finally:
        try:
            # The children may exit between the check and the kill call.
            # Ignore any exception raised here.
            for c in multiprocessing.active_children():
                # Send the signal to the whole process group.
                # Otherwise some twistd sub-childs can remain alive.
                os.killpg(os.getpgid(c.pid), signal.SIGKILL)
        except:
            pass
        if not no_clean:
            clean(base_workdir_temp)
        sys.exit(0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--config-dir', help='Path to the directory of the build master config files. '
                        'Defauls to the directory where this script is located.',
                        dest='configdir', type=str,
                        default=os.path.dirname(__file__))
    parser.add_argument('--base-temp-dir', help='Path where the temporal working directory will be created. '
                        'Note: To trigger test builds with the test workers you need enough free space on that path.',
                        dest='basetempdir', default=None, type=str)
    parser.add_argument('--no-clean', help='Do not clean the temporal working dir on exit.',
                        dest='no_clean', action='store_true')
    parser.add_argument('--no-workers', help='Do not start the test workers.',
                        dest='no_workers', action='store_true')
    parser.add_argument('--use-system-version', help='Instead of setting up a virtualenv with the buildbot version '
                        'used by build.webkit.org, use the buildbot version installed on this system.',
                        dest='use_system_version', action='store_true')
    args = parser.parse_args()
    main(args.configdir, args.basetempdir, args.no_clean, args.no_workers, args.use_system_version)
