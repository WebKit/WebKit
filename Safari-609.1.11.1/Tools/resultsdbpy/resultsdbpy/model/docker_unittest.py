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

import mock

from resultsdbpy.model.docker import Docker
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class DockerUnittest(WaitForDockerTestCase):

    @WaitForDockerTestCase.run_if_has_docker()
    def test_start(self):
        with Docker.instance():
            self.assertTrue(Docker.is_running())

    @WaitForDockerTestCase.run_if_has_docker()
    def test_stack(self):
        with Docker.instance():
            def subprocess_callback(*args, **kwargs):
                return self.fail('Docker instances not correctly stacking')

            # The test here is that Docker is correctly stacking and no subprocess calls are being made after Docker is already running.
            with mock.patch('subprocess.check_call', new=subprocess_callback), mock.patch('subprocess.check_output', new=subprocess_callback):
                with Docker.instance():
                    pass

    @WaitForDockerTestCase.run_if_has_docker()
    def test_project_running(self):
        with Docker.instance():
            self.assertTrue(Docker.is_project_running(Docker.DEFAULT_PROJECT))
