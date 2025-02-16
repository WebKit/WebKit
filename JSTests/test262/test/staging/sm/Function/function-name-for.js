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
var BUGNUMBER = 883377;
var summary = "Anonymous function name should be set based on for-in initializer";

print(BUGNUMBER + ": " + summary);

var exprs = [
    ["function() {}", false],
    ["function named() {}", true],
    ["function*() {}", false],
    ["function* named() {}", true],
    ["async function() {}", false],
    ["async function named() {}", true],
    ["() => {}", false],
    ["async () => {}", false],
    ["class {}", false],
    ["class named {}", true],
];

function testForInHead(expr, named) {
    eval(`
    for (var forInHead = ${expr} in {}) {
    }
    `);
    assert.sameValue(forInHead.name, named ? "named" : "forInHead");
}
for (var [expr, named] of exprs) {
    testForInHead(expr, named);
}

