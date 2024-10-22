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

import unittest

from webkitcorepy import Docker, OutputCapture, mocks


class DockerTest(unittest.TestCase):
    DOCKER = mocks.Subprocess(
        mocks.Subprocess.Route(
            '/bin/docker', '-v',
            completion=mocks.ProcessCompletion(
                returncode=0,
                stdout='Docker version 20.10.17, build 100c701',
            ),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'images', completion=mocks.ProcessCompletion(
                returncode=0,
                stdout='REPOSITORY     TAG    IMAGE ID     CREATED     SIZE\n'
                       'cassandra      latest 7772bea61223 3 weeks ago 146MB\n'
                       'redis          latest ea30bef6a142 3 weeks ago 39MB\n',
            ),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'pull', 'nginx:latest', completion=mocks.ProcessCompletion(returncode=0),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'pull', completion=mocks.ProcessCompletion(returncode=1),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'container', 'ls', '-a',
            completion=mocks.ProcessCompletion(
                returncode=0,
                stdout='ID        IMAGE            OS    ARCH  STATE\n'
                       'cassandra cassandra:latest linux arm64 RUNNING\n'
                       'redis     redis:latest     linux arm64 STOPPED\n',
            ),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'container', 'create', '--name', 'nginx', '--rm', 'nginx:latest',
            completion=mocks.ProcessCompletion(returncode=0),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'container', 'start', 'redis', completion=mocks.ProcessCompletion(returncode=0),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'container', 'stop', 'cassandra', completion=mocks.ProcessCompletion(returncode=0),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'container', completion=mocks.ProcessCompletion(returncode=1),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'volume', 'ls', completion=mocks.ProcessCompletion(
                returncode=0,
                stdout='NAME          METADATA OPTIONS\n'
                       'cassandradata          {"format":"ext4"}\n',
            ),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'volume', 'create', 'redisdata', completion=mocks.ProcessCompletion(returncode=0),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'volume', 'rm', 'cassandradata', completion=mocks.ProcessCompletion(returncode=0),
        ), mocks.Subprocess.Route(
            '/bin/docker', 'volume', completion=mocks.ProcessCompletion(returncode=1),
        ), mocks.Subprocess.Route(
            '/bin/docker', completion=mocks.ProcessCompletion(returncode=0),
        ), ordered=True,
    )

    @classmethod
    def mock_which(cls, **programs):
        from mock import patch
        return patch('whichcraft.which', new=lambda arg: programs.get(arg, None))

    def test_command(self):
        with self.mock_which():
            self.assertIsNone(Docker.command(cached=False))

        with self.mock_which(docker='/bin/docker'):
            self.assertEqual('/bin/docker', Docker.command(cached=False))

    def test_installed(self):
        with OutputCapture() as captured, self.mock_which(), self.DOCKER:
            self.assertFalse(Docker.installed(cached=False))
        self.assertEqual('Nothing capable of running docker containers detected\n', captured.stderr.getvalue())

        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertTrue(Docker.installed(cached=False))

    def test_images(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertDictEqual(Docker.images(cached=False), dict(
                cassandra={
                    'created': '3 weeks ago',
                    'image id': '7772bea61223',
                    'size': '146MB',
                    'tag': 'latest',
                }, redis={
                    'created': '3 weeks ago',
                    'image id': 'ea30bef6a142',
                    'size': '39MB',
                    'tag': 'latest',
                },
            ))

    def test_downloaded(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertTrue(Docker.Container('cassandra').downloaded(cached=False))
            self.assertTrue(Docker.Container('redis').downloaded(cached=False))
            self.assertFalse(Docker.Container('nginx').downloaded(cached=False))

    def test_download(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertIsNone(Docker.Container('cassandra').download(cached=False))
            self.assertTrue(Docker.Container('nginx').download(cached=False))
            self.assertFalse(Docker.Container('python3').download(cached=False))

    def test_containers(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertDictEqual(Docker.containers(cached=False), dict(
                cassandra=dict(
                    image='cassandra:latest',
                    arch='arm64',
                    os='linux',
                    state='RUNNING',
                ), redis=dict(
                    image='redis:latest',
                    arch='arm64',
                    os='linux',
                    state='STOPPED',
                ),
            ))

    def test_created(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertTrue(Docker.Container('cassandra').created(cached=False))
            self.assertTrue(Docker.Container('redis').created(cached=False))
            self.assertFalse(Docker.Container('nginx').created(cached=False))

    def test_create(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertIsNone(Docker.Container('cassandra').create(cached=False))
            self.assertTrue(Docker.Container('nginx').create(cached=False))
            self.assertFalse(Docker.Container('python3').create(cached=False))

    def test_running(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertTrue(Docker.Container('cassandra').is_running(cached=False))
            self.assertFalse(Docker.Container('redis').is_running(cached=False))
            self.assertFalse(Docker.Container('nginx').is_running(cached=False))

    def test_start(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertIsNone(Docker.Container('cassandra').start(cached=False))
            self.assertTrue(Docker.Container('redis').start(cached=False))
            self.assertFalse(Docker.Container('nginx').start(cached=False))

    def test_stop(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertTrue(Docker.Container('cassandra').stop(cached=False))
            self.assertIsNone(Docker.Container('redis').stop(cached=False))
            self.assertIsNone(Docker.Container('nginx').stop(cached=False))

    def test_container_decorator(self):
        with OutputCapture() as captured, self.mock_which(docker='/bin/docker'), self.DOCKER:
            container = Docker.Container('cassandra')
            container.__enter__(cached=False)
            self.assertFalse(container.owner)
            container.__exit__(cached=False)

            container = Docker.Container('redis')
            container.__enter__(cached=False)
            self.assertTrue(container.owner)
            container.__exit__(cached=False)
            self.assertFalse(container.owner)

            container = Docker.Container('nginx')
            container.__enter__(cached=False)
            container.__exit__(cached=False)

        self.assertEqual("Failed to start container 'nginx'\n", captured.stderr.getvalue())

    def test_volume(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertDictEqual(Docker.volumes(cached=False), dict(
                cassandradata=dict(
                    metadata='',
                    options='{"format":"ext4"}',
                ),
            ))

    def test_volume_create(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertIsNone(Docker.Volume('cassandradata').create(cached=False))
            self.assertTrue(Docker.Volume('redisdata').create(cached=False))

    def test_volume_remove(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            self.assertTrue(Docker.Volume('cassandradata').remove(cached=False))
            self.assertIsNone(Docker.Volume('redisdata').remove(cached=False))

    def test_volume_decorator(self):
        with self.mock_which(docker='/bin/docker'), self.DOCKER:
            volume = Docker.Volume('cassandradata')
            volume.__enter__(cached=False)
            self.assertFalse(volume.owner)
            volume.__exit__(cached=False)

            volume = Docker.Volume('redisdata')
            volume.__enter__(cached=False)
            self.assertTrue(volume.owner)
            volume.__exit__(cached=False)
            self.assertFalse(volume.owner)
