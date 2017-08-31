/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
"use strict";

load("All.js");

function checkInt(result, expected)
{
    if (!(result instanceof EInt))
        throw new Error("Wrong result type; result: " + result);
    if (result.value != expected)
        throw new Error("Wrong result: " + result + " (expected " + expected + ")");
}

function TEST_add1() {
    let program = prepare("<test>", 0, "int foo(int x) { return x + 1; }");
    checkInt(callFunction(program, "foo", [], [new EInt(program.intrinsics.int32, 42)]), 43);
}

function TEST_simpleGeneric() {
    let program = prepare(
        "<test>", 0,
        `T id<T>(T x) { return x; }
         int foo(int x) { return id(x) + 1; }`);
    checkInt(callFunction(program, "foo", [], [new EInt(program.intrinsics.int32, 42)]), 43);
}

let before = preciseTime();

let filter = /.*/; // run everything by default
if (this["arguments"]) {
    for (let i = 0; i < arguments.length; i++) {
        switch (arguments[0]) {
        case "--filter":
            filter = new RegExp(arguments[++i]);
            break;
        default:
            throw new Error("Unknown argument: ", arguments[i]);
        }
    }
}


for (let s in this) {
    if (s.startsWith("TEST_") && s.match(filter)) {
        print(s + "...");
        this[s]();
        print("    OK!");
    }
}

let after = preciseTime();

print("That took " + (after - before) * 1000 + " ms.");
