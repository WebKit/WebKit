# Copyright (C) 2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import atexit
import os
import re
import socket
import subprocess
import time

from cassandra.cluster import Cluster, NoHostAvailable


class Docker(object):

    DOCKER_BIN = '/usr/local/bin/docker'
    DOCKER_COMPOSE = '/usr/local/bin/docker-compose'
    DEFAULT_PROJECT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'docker-compose.yml')
    PS_PORT = re.compile(r'0.0.0.0:(?P<port>\d+)\-\>\d+/tcp')

    _has_docker = None
    _docker_stack = 0
    _project_stack = {}
    _instance_for_project = {}

    @classmethod
    def installed(cls):
        if cls._has_docker is not None:
            return cls._has_docker

        try:
            with open(os.devnull, 'w') as devnull:
                subprocess.check_call([cls.DOCKER_BIN, '-v'], stdout=devnull, stderr=devnull)
            cls._has_docker = True
        except (OSError, subprocess.CalledProcessError):
            print('Docker not installed, required resources will be mocked. Download it at https://download.docker.com/mac/stable/Docker.dmg')
            cls._has_docker = False
        return cls._has_docker

    @classmethod
    def is_running(cls):
        try:
            with open(os.devnull, 'w') as devnull:
                subprocess.check_call([cls.DOCKER_BIN, 'container', 'ls', '-a'], stdout=devnull, stderr=devnull)
            return True
        except (OSError, subprocess.CalledProcessError):
            return False

    @classmethod
    def start(cls, timeout=60):
        if cls.is_running():
            return

        if not cls.installed():
            raise RuntimeError('Docker not installed')

        with open(os.devnull, 'w') as devnull:
            subprocess.check_call(['/usr/bin/open', '--background', '-a', 'Docker'], stdout=devnull, stderr=devnull)

        deadline = None if timeout is None else (time.time() + timeout)
        while deadline is None or time.time() < deadline:
            if cls.is_running():
                return
            time.sleep(3)
        raise RuntimeError('Docker failed to start')

    @classmethod
    def stop(cls):
        if not cls.is_running():
            return
        with open(os.devnull, 'w') as devnull:
            try:
                subprocess.check_call(['/usr/bin/killall', 'Docker'], stdout=devnull, stderr=devnull)
            except subprocess.CalledProcessError:
                raise RuntimeError('Failed to shutdown Docker, maybe it was in the process of quiting?')

    @classmethod
    def start_project(cls, project):
        project = os.path.abspath(project)
        if not os.path.isfile(project):
            raise RuntimeError(f'Cannot find Docker project: {project}')
        if not cls.is_running():
            raise RuntimeError('Cannot start project if Docker is not running')

        with open(os.devnull, 'w') as devnull:
            subprocess.check_call([cls.DOCKER_COMPOSE, '-f', project, 'up', '-d', '--quiet-pull', '--no-color'], stdout=devnull, stderr=devnull)

    @classmethod
    def wait_for_project(cls, project):
        project = os.path.abspath(project)
        if not os.path.isfile(project):
            raise RuntimeError(f'Cannot find Docker project: {project}')
        if not cls.is_running():
            raise RuntimeError(f'Docker is not running, cannot wait for {project}')

        has_cassandra = False
        ports = []
        with open(os.devnull, 'w') as devnull:
            output = subprocess.check_output([cls.DOCKER_COMPOSE, '-f', project, 'ps'], stderr=devnull).decode('utf-8')
            if len(output.splitlines()) <= 2:
                raise RuntimeError(f'{project} has not been started, cannot wait for it')
            for line in output.splitlines()[2:]:
                if 'cassandra' in line:
                    has_cassandra = True
                for match in cls.PS_PORT.findall(line):
                    ports.append(int(match))

        while True:
            all_ports_open = True
            for port in ports:
                soc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                try:
                    soc.connect(('localhost', port))
                    soc.shutdown(2)
                    continue
                except BaseException:
                    all_ports_open = False
                    break
            if all_ports_open:
                try:
                    if has_cassandra:
                        connection = Cluster(['localhost']).connect()
                        connection.cluster.shutdown()
                    return
                except NoHostAvailable:
                    pass
            time.sleep(3)

    @classmethod
    def is_project_running(cls, project):
        project = os.path.abspath(project)
        if not os.path.isfile(project):
            raise RuntimeError(f'Cannot find Docker project: {project}')

        if not cls.is_running():
            return False
        with open(os.devnull, 'w') as devnull:
            output = subprocess.check_output([cls.DOCKER_COMPOSE, '-f', project, 'ps'], stderr=devnull)
            return len(output.splitlines()) > 2

    @classmethod
    def stop_project(cls, project):
        project = os.path.abspath(project)
        if not os.path.isfile(project):
            raise RuntimeError(f'Cannot find Docker project: {project}')
        if not cls.is_running():
            raise RuntimeError('Cannot stop project if Docker is not running')

        with open(os.devnull, 'w') as devnull:
            subprocess.check_call([cls.DOCKER_COMPOSE, '-f', project, 'down'], stdout=devnull, stderr=devnull)

    @classmethod
    def instance(cls, project=DEFAULT_PROJECT):
        project = os.path.abspath(project)
        if project not in cls._instance_for_project:
            cls._instance_for_project[project] = cls(project)
            cls._instance_for_project[project].__enter__()
            atexit.register(cls._instance_for_project[project].__exit__)
        return cls._instance_for_project[project]

    def __init__(self, project=DEFAULT_PROJECT):
        if not self.installed():
            raise RuntimeError('Cannot manage a Docker instance if Docker is not installed')
        self.project = os.path.abspath(project)
        if not os.path.isfile(self.project):
            raise RuntimeError(f'Cannot find Docker project: {self.project}')

    def __enter__(self):
        if Docker._docker_stack == 0:
            if Docker.is_running():
                Docker._docker_stack += 1
            else:
                Docker.start()
        Docker._docker_stack += 1

        if Docker._project_stack.get(self.project, 0) == 0:
            if Docker.is_project_running(self.project):
                Docker._project_stack[self.project] = Docker._project_stack.get(self.project, 0) + 1
            else:
                Docker.start_project(self.project)
            Docker.wait_for_project(self.project)
        Docker._project_stack[self.project] = Docker._project_stack.get(self.project, 0) + 1

    def __exit__(self, *args):
        Docker._project_stack[self.project] = Docker._project_stack.get(self.project, 0) - 1
        if Docker._project_stack[self.project] <= 0:
            Docker.stop_project(self.project)

        Docker._docker_stack -= 1
        if Docker._docker_stack <= 0:
            Docker.stop()
