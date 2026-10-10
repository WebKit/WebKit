# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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


import attr

from .test import Test, Reference


@attr.s(frozen=True, slots=True)
class ReferenceInput(object):
    reference = attr.ib(type=Reference)
    url = attr.ib(type=str)


@attr.s(frozen=True, slots=True)
class TestInput(object):
    """Information about a test needed to run it.

    This differs from a Test object insofar as it contains metadata not specific to the test,
    derived from TestExpectations/test execution options (e.g., timeout).
    """
    test = attr.ib(type=Test)  # ordering delegates to Test's __lt__ etc.
    url = attr.ib(default=None, type=str, order=False)
    reference_inputs = attr.ib(default=(), type=tuple, order=False)
    timeout = attr.ib(default=None, order=False)  # type: Union[None, int, str]
    is_slow = attr.ib(default=None, order=False)  # type: Optional[bool]
    should_dump_jsconsolelog_in_stderr = attr.ib(default=None, order=False)  # type: Optional[bool]
    should_run_pixel_test = attr.ib(default=None, order=False)  # type: Optional[bool]

    @property
    def needs_servers(self):
        return self.test.needs_any_server

    @property
    def test_name(self):
        return self.test.test_path
