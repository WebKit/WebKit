# Copyright (C) 2022 Apple Inc. All rights reserved.
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

import os
import time
import sys

from datetime import datetime

from webkitcorepy import OutputCapture, Terminal, testing
from webkitscmpy import program, mocks


class TestShow(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestShow, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as mocked, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            commit = mocked.commits['main'][-1]
            self.assertEqual(-1, program.main(
                args=('show', 'main'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '''commit 5@main (d8bce26fa65c6fc8f39c17927abb77f69fab82fc)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    Patch Series

diff --git a/Source/main.cpp b/Source/main.cpp
index 2deba859a126..7b85f5cecd66 100644
--- a/Source/main.cpp
--- b/Source/main.cpp
@@ -2948,6 +2948,8 @@ Vector<CompositedClipData> RenderLayerCompositor::computeAncestorClippingStack(c
     auto backgroundClip = clippedLayer.backgroundClipRect(RenderLayer::ClipRectsContext(&clippingRoot, TemporaryClipRects, options));
     ASSERT(!backgroundClip.affectedByRadius());
     auto clipRect = backgroundClip.rect();
+    if (clipRect.isInfinite())
+        return;
    auto offset = layer.convertToLayerCoords(&clippingRoot, {{ }}, RenderLayer::AdjustForColumns);
    clipRect.moveBy(-offset);
'''.format(datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000')),
        )

    def test_git_svn(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True) as mocked, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            commit = mocked.commits['main'][-1]
            self.assertEqual(-1, program.main(
                args=('show', 'main'),
                path=self.path,
            ))

        self.maxDiff = None
        self.assertEqual(
            captured.stdout.getvalue(),
            '''commit 5@main (d8bce26fa65c6fc8f39c17927abb77f69fab82fc, r9)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    Patch Series
    git-svn-id: https://svn.example.org/repository/repository/trunk@9 268f45cc-cd09-0410-ab3c-d52691b4dbfc

diff --git a/Source/main.cpp b/Source/main.cpp
index 2deba859a126..7b85f5cecd66 100644
--- a/Source/main.cpp
--- b/Source/main.cpp
@@ -2948,6 +2948,8 @@ Vector<CompositedClipData> RenderLayerCompositor::computeAncestorClippingStack(c
     auto backgroundClip = clippedLayer.backgroundClipRect(RenderLayer::ClipRectsContext(&clippingRoot, TemporaryClipRects, options));
     ASSERT(!backgroundClip.affectedByRadius());
     auto clipRect = backgroundClip.rect();
+    if (clipRect.isInfinite())
+        return;
    auto offset = layer.convertToLayerCoords(&clippingRoot, {{ }}, RenderLayer::AdjustForColumns);
    clipRect.moveBy(-offset);
'''.format(datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000')),
        )

    def test_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(1, program.main(
                args=('show',),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), "Can only 'show' on a native Git repository\n")

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(1, program.main(
                args=('show',),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), "Can only 'show' on a native Git repository\n")
