// Copyright (C) 2012-2013 Ecma International
// Copyright (C) 2016 André Bargull. All rights reserved.
// Copyright (C) 2020 Apple Inc. All rights reserved.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
// following conditions are met:
// 1.   Redistributions of source code must retain the above copyright notice, this list of conditions and the following
//      disclaimer.
// 2.   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//      following disclaimer in the documentation and/or other materials provided with the distribution.
// 3.   Neither the name of the authors nor Ecma International may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE ECMA INTERNATIONAL "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL ECMA INTERNATIONAL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var ThrowTypeError = Object.getOwnPropertyDescriptor(function() {
    "use strict";
    return arguments;
}(), "callee").get;

function testUnmappedArguments(args)
{
    var unmappedCalleeDesc = Object.getOwnPropertyDescriptor(args, "callee");
    shouldBe(ThrowTypeError, unmappedCalleeDesc.get);
    shouldBe(ThrowTypeError, unmappedCalleeDesc.set);
}

function testMappedArguments(args)
{
    var unmappedCalleeDesc = Object.getOwnPropertyDescriptor(args, "callee");
    shouldBe(unmappedCalleeDesc.value !== undefined, true);
}

function argumentGenerator1(a = 0) {
    return arguments;
}
testUnmappedArguments(argumentGenerator1());

function argumentGenerator2() {
    function inner(a = 0) {
        return arguments;
    }
    return inner();
}
testUnmappedArguments(argumentGenerator2());

function argumentGenerator3() {
    function inner(a = 0) {
        return arguments;
    }
    return arguments;
}
testMappedArguments(argumentGenerator3());

function argumentGenerator4(a = 0) {
    function inner() {
        return arguments;
    }
    return inner();
}
testMappedArguments(argumentGenerator4());

function argumentGenerator5() {
    function inner() {
        function inner2(a = 0) {
            return arguments;
        }
        return inner2();
    }
    return inner();
}
testUnmappedArguments(argumentGenerator5());

function argumentGenerator6(...a) {
    return arguments;
}
testUnmappedArguments(argumentGenerator6());

function argumentGenerator7() {
    function inner(...a) {
        return arguments;
    }
    return inner();
}
testUnmappedArguments(argumentGenerator7());

function argumentGenerator8() {
    function inner(...a) {
        return arguments;
    }
    return arguments;
}
testMappedArguments(argumentGenerator8());

function argumentGenerator9(...a) {
    function inner() {
        return arguments;
    }
    return inner();
}
testMappedArguments(argumentGenerator9());

function argumentGenerator10() {
    function inner() {
        function inner2(...a) {
            return arguments;
        }
        return inner2();
    }
    return inner();
}
testUnmappedArguments(argumentGenerator10());
