/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// Regression test for https://bugs.webkit.org/show_bug.cgi?id=144152

// The bug in 144152 needs 3 conditions to manifest:
// 1. The branch test value in the inlined function is of useKind ObjectOrOtherUse.
// 2. The branch test value is proven to be a known useKind.
// 3. The masqueradesAsUndefined watchpoint is no longer valid.
// With the bug fixed, this test should not crash on debug builds.

function inlinedFunction(x) {
    if (x) // Conditional branch that will assert on a debug build if the bug is present.
        new Object;
}

function foo(x) {
    if (x) // Testing x before calling the inlined function sets up condition 2.
        inlinedFunction(x);
}

makeMasquerader(); // Invalidates the masqueradesAsUndefined watchpoint for condition 3.
for (var i = 0; i < 10000; i++)
    foo({}); // Pass an object argument to set up condition 1.
