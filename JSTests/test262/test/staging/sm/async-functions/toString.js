// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1185106;
var summary = "async function toString";

print(BUGNUMBER + ": " + summary);

async function f1(a, b, c) { await a; }

assert.sameValue(f1.toString(),
         "async function f1(a, b, c) { await a; }");

assert.sameValue(async function (a, b, c) { await a; }.toString(),
         "async function (a, b, c) { await a; }");

assert.sameValue((async (a, b, c) => await a).toString(),
         "async (a, b, c) => await a");

assert.sameValue((async (a, b, c) => { await a; }).toString(),
         "async (a, b, c) => { await a; }");

assert.sameValue({ async foo(a, b, c) { await a; } }.foo.toString(),
         "async foo(a, b, c) { await a; }");

