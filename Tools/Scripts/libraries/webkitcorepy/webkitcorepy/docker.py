# Copyright (C) 2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys
import time

from webkitcorepy import decorators, run


class Docker(object):
    _installed = None
    _command = None
    DOCKER_APP = '/Applications/Docker.app'
    TIMEOUT = 10

    class Container(object):
        def __init__(self, image, name=None, tag='latest', remove=True, ports=None, port=None, volumes=None):
            self.image = image
            self.name = name or image
            self.tag = tag

            self.remove = remove
            self.owner = False
            self.ports = ports or {}
            self.volumes = volumes or []
            if port:
                self.ports[port] = port

        def __enter__(self, cached=True):
            for volume in self.volumes:
                volume.__enter__(cached=cached)

            did_start = self.start(cached=cached)
            self.owner = bool(did_start)
            if did_start is False:
                sys.stderr.write("Failed to start container '{}'\n".format(self.name))

        def __exit__(self, *args, **kwargs):
            cached = kwargs.get('cached', True)
            if self.owner and self.stop(cached=cached) is False:
                sys.stderr.write("Failed to stop container '{}'\n".format(self.name))
            self.owner = False

            for volume in self.volumes:
                volume.__exit__(*args, **kwargs)

        def download(self, cached=True, capture_output=True):
            if self.downloaded(cached=cached):
                return None
            return not run(
                [Docker.command(cached=cached), 'pull', '{}:{}'.format(self.image, self.tag)],
                capture_output=capture_output,
            ).returncode

        def downloaded(self, cached=True):
            return self.tag == Docker.images(cached=cached).get(self.image, {}).get('tag', None)

        def created(self, cached=True):
            return self.name in Docker.containers(cached=cached)

        def create(self, cached=True, capture_output=True):
            if self.created(cached=cached):
                return None

            args = ['--name', self.name]
            if self.remove:
                args.append('--rm')
            for port, mapping in self.ports.items():
                args.append('-p')
                args.append('{}:{}'.format(port, mapping))
            for volume in self.volumes:
                args.append('--volume')
                args.append(volume.name)

            return not run(
                [Docker.command(cached=cached), 'container', 'create'] + args + ['{}:{}'.format(self.image, self.tag)],
                capture_output=capture_output,
            ).returncode

        def start(self, cached=True, capture_output=True):
            if self.is_running(cached=cached):
                return None
            self.download(cached=cached, capture_output=capture_output)
            self.create(cached=cached, capture_output=capture_output)
            return not run(
                [Docker.command(cached=cached), 'container', 'start', self.name],
                capture_output=capture_output,
            ).returncode

        def stop(self, cached=True, capture_output=True):
            if not self.is_running(cached=cached):
                return None
            return not run(
                [Docker.command(cached=cached), 'container', 'stop', self.name],
                capture_output=capture_output,
            ).returncode

        def is_running(self, cached=True):
            return 'RUNNING' == Docker.containers(cached=cached).get(self.name, {}).get('state', 'STOPPED')

    class Volume(object):
        def __init__(self, name):
            self.name = name
            self.owner = False

        def __enter__(self, cached=True):
            did_create = self.create(cached=cached)
            self.owner = bool(did_create)
            if did_create is False:
                sys.stderr.write("Failed to create volume '{}'\n".format(self.name))

        def __exit__(self, *args, **kwargs):
            cached = kwargs.get('cached', True)
            if self.owner and self.remove(cached=cached) is False:
                sys.stderr.write("Failed to remove volume '{}'\n".format(self.name))
            self.owner = False

        def exists(self, cached=True):
            return bool(Docker.volumes(cached=cached).get(self.name, False))

        def create(self, cached=True):
            if self.exists():
                return None
            return not run(
                [Docker.command(cached=cached), 'volume', 'create', self.name],
                capture_output=True,
            ).returncode

        def remove(self, cached=True):
            if not self.exists():
                return None
            return not run(
                [Docker.command(cached=cached), 'volume', 'rm', self.name],
                capture_output=True,
            ).returncode

    @classmethod
    def command(cls, cached=True):
        if cached and cls._command is not None:
            return cls._command

        from whichcraft import which
        for candidate in [which('docker')]:
            if candidate:
                if cached:
                    cls._command = candidate
                return candidate
        return None

    @classmethod
    def installed(cls, log=True, cached=True):
        if cached and cls._installed is not None:
            return cls._installed

        result = False
        if cls.command(cached=cached):
            if not run([cls.command(cached=cached), '-v'], capture_output=True).returncode:
                result = True
        elif log:
            sys.stderr.write('Nothing capable of running docker containers detected\n')

        if cached:
            cls._installed = result

        return result

    @classmethod
    def opened(cls, cached=True):
        return not run([cls.command(cached=cached), 'images'], capture_output=True).returncode

    @classmethod
    def open(cls, cached=True, timeout=10):
        if cls.opened(cached=cached):
            return

        cmd = cls.command(cached=cached)
        if not cmd:
            raise RuntimeError('No Docker harness to open')

        if sys.platform == 'darwin':
            if 'docker' in cmd:
                app = cls.DOCKER_APP
            else:
                raise NotImplementedError("'{}' is not recognized".format(cmd))
            if not os.path.isdir(app):
                raise OSError("'{}' is not installed".format(app))
            if run(['open', app], capture_output=True).returncode:
                raise RuntimeError("Failed to open '{}'".format(app))

            # Wait until application has started
            deadline = time.time() + timeout
            while time.time() < deadline:
                if cls.opened(cached=cached):
                    return
                time.sleep(0.1)
            raise RuntimeError("Failed to verify '{}' has started".format(app))
        raise NotImplementedError('Cannot start Docker on this platform')

    @classmethod
    def parse_output(cls, lines):
        keys = ['']
        length = [0]
        whitespace_count = 0
        if not lines:
            return {}
        for c in lines[0]:
            if c.isspace():
                whitespace_count += 1
            elif whitespace_count == 1 and len(keys[-1]) < 7:
                keys[-1] += ' ' + c.lower()
                whitespace_count = 0
            elif whitespace_count:
                keys.append(c.lower())
                length.append(0)
                whitespace_count = 0
            else:
                keys[-1] += c.lower()
            length[-1] += 1
        length[-1] = 0

        result = {}
        for line in lines[1:]:
            values = []
            start = 0
            for cnt in length:
                if cnt:
                    values.append(line[start:start + cnt].strip())
                else:
                    values.append(line[start:].strip())
                start += cnt
            if not values:
                continue
            key = values[0]
            result[key] = {keys[cnt + 1]: values[cnt + 1] for cnt in range(len(keys) - 1)}
        return result

    @classmethod
    def images(cls, cached=True):
        proc = run([cls.command(cached=cached), 'images'], encoding='utf-8', capture_output=True)
        if proc.returncode:
            return {}
        result = {}
        for key, values in cls.parse_output(proc.stdout.splitlines()).items():
            if ':' in key:
                key, tag = key.split(':', 1)
                values['tag'] = tag
            if 'digest' in values:
                values['digest'] = values['digest'].strip('.')
            result[key] = values
        return result

    @classmethod
    def containers(cls, cached=True):
        proc = run([cls.command(cached=cached), 'container', 'ls', '-a'], encoding='utf-8', capture_output=True)
        if proc.returncode:
            return {}
        return cls.parse_output(proc.stdout.splitlines())

    @classmethod
    def volumes(cls, cached=True):
        proc = run([cls.command(cached=cached), 'volume', 'ls'], encoding='utf-8', capture_output=True)
        if proc.returncode:
            return {}
        return cls.parse_output(proc.stdout.splitlines())
