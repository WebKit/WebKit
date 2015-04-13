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

// Regression test for https://bugs.webkit.org/show_bug.cgi?id=143407.

var verbose = false;

function foo() {
    return arguments.length;
}

function Foo() {
    this.length = arguments.length;
}

var callTestBodyStr =
"    var result = this.method.apply(this, arguments);" + "\n" +
"    return result + 1;";

var constructTestBodyStr =
"    return new this.constructor(...arguments);";

var tiers = [
    { name: "LLint", iterations: 10 },
    { name: "BaselineJIT", iterations: 50 },
    { name: "DFG", iterations: 500 },
    { name: "FTL", iterations: 10000 },
];

function doTest(testCategory, testBodyStr, tier) {
    try {
        var iterations = tiers[tier].iterations;
        if (verbose)
            print("Testing " + testCategory + " tier " + tiers[tier].name + " by iterating " + iterations + " times");

        var o = {}
        o.method = foo;
        o.constructor = Foo;
        o.trigger = new Function(testBodyStr);

        for (var i = 0; i < iterations; i++)
            o.trigger(o, 1);
        o.trigger(o, 1, 2);

    } catch (e) {
        print("FAILED " + testCategory + " in tier " + tiers[tier].name + ": " + e);
        return false;
    }
    return true;
}

var failureFound = 0;

for (var tier = 0; tier < tiers.length; tier++) {
    if (!doTest("op_call_varargs", callTestBodyStr, tier))
        failureFound++;
}

for (var tier = 0; tier < tiers.length; tier++) {
    if (!doTest("op_construct_varargs", constructTestBodyStr, tier))
        failureFound++;
}

if (failureFound == 1)
    throw "ERROR: test has 1 failure";
else if (failureFound > 1)
    throw "ERROR: test has " + failureFound + " failures";
else if (verbose)
    print("No failures");
