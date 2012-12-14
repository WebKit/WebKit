# Copyright (C) 2012 Google Inc. All rights reserved.
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

import unittest

from webkitpy.common.system.systemhost_mock import MockSystemHost

from .profiler import ProfilerFactory, GooglePProf


class ProfilerFactoryTest(unittest.TestCase):
    def test_basic(self):
        host = MockSystemHost()
        self.assertFalse(host.filesystem.exists("/tmp/output"))
        profiler = ProfilerFactory.create_profiler(host, '/bin/executable', '/tmp/output')
        self.assertTrue(host.filesystem.exists("/tmp/output"))
        self.assertEquals(profiler._output_path, "/tmp/output/test.dtps")

        host.platform.os_name = 'linux'
        profiler = ProfilerFactory.create_profiler(host, '/bin/executable', '/tmp/output')
        self.assertEquals(profiler._output_path, "/tmp/output/test.data")

    def test_pprof_output_regexp(self):
        pprof_output = """
sometimes
there
is
junk before the total line


Total: 3770 samples
      76   2.0%   2.0%      104   2.8% lookup (inline)
      60   1.6%   3.6%       60   1.6% FL_SetPrevious (inline)
      56   1.5%   5.1%       56   1.5% MaskPtr (inline)
      51   1.4%   6.4%      222   5.9% WebCore::HTMLTokenizer::nextToken
      42   1.1%   7.6%       47   1.2% WTF::Vector::shrinkCapacity
      35   0.9%   8.5%       35   0.9% WTF::RefPtr::get (inline)
      33   0.9%   9.4%       43   1.1% append (inline)
      29   0.8%  10.1%       67   1.8% WTF::StringImpl::deref (inline)
      29   0.8%  10.9%      100   2.7% add (inline)
      28   0.7%  11.6%       28   0.7% WebCore::QualifiedName::localName (inline)
      25   0.7%  12.3%       27   0.7% WebCore::Private::addChildNodesToDeletionQueue
      24   0.6%  12.9%       24   0.6% __memcpy_ssse3_back
      23   0.6%  13.6%       23   0.6% intHash (inline)
      23   0.6%  14.2%       76   2.0% tcmalloc::FL_Next
      23   0.6%  14.8%       95   2.5% tcmalloc::FL_Push
      22   0.6%  15.4%       22   0.6% WebCore::MarkupTokenizerBase::InputStreamPreprocessor::peek (inline)
"""
        expected_first_ten_lines = """      76   2.0%   2.0%      104   2.8% lookup (inline)
      60   1.6%   3.6%       60   1.6% FL_SetPrevious (inline)
      56   1.5%   5.1%       56   1.5% MaskPtr (inline)
      51   1.4%   6.4%      222   5.9% WebCore::HTMLTokenizer::nextToken
      42   1.1%   7.6%       47   1.2% WTF::Vector::shrinkCapacity
      35   0.9%   8.5%       35   0.9% WTF::RefPtr::get (inline)
      33   0.9%   9.4%       43   1.1% append (inline)
      29   0.8%  10.1%       67   1.8% WTF::StringImpl::deref (inline)
      29   0.8%  10.9%      100   2.7% add (inline)
      28   0.7%  11.6%       28   0.7% WebCore::QualifiedName::localName (inline)
"""
        host = MockSystemHost()
        profiler = GooglePProf(host, '/bin/executable', '/tmp/output')
        self.assertEquals(profiler._first_ten_lines_of_profile(pprof_output), expected_first_ten_lines)
