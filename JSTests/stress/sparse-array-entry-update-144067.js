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

// Regression test for https://bugs.webkit.org/show_bug.cgi?id=144067.
// This test aims to continually override the setter in a sparse array object, and
// trigger GCs to give it a chance to collect the newly set entry value if the bug exists. 
// With the bug fixed, this test should not crash.

var data = {};
var sparseObj = {};

for (var i = 0; i < 5; i++)
    sparseObj[i] = i;

function useMemoryToTriggerGCs() {
    var arr = [];
    var limit = DFGTrue() ? 10000 : 100;
    for (var i = 0; i < limit; i++)
        arr[i] = { a: "using" + i, b: "up" + i, c: "memory" + i };
    return arr;
}

function foo(x) {
    if (!x)
        return;
    data.textContent = sparseObj.__defineSetter__("16384", foo);
    for (var i = 0; i < 10; i++)
        sparseObj.__defineSetter__("" + (16384 + i), foo);
    useMemoryToTriggerGCs();
    sparseObj[16384] = x - 1;
}

var recursionDepthNeededToTriggerTheFailure = 100;
foo(recursionDepthNeededToTriggerTheFailure);
