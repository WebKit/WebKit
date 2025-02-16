// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1184922;
var summary = "Array destructuring with various default values in various context - function expression with nested objects";

print(BUGNUMBER + ": " + summary);

testDestructuringArrayDefault("function f() { return { f() {}, *g() {}, r: /a/ }; }");
testDestructuringArrayDefault("function* g() { return { f() {}, *g() {}, r: /b/ }; }");
testDestructuringArrayDefault("() => { return { f() {}, *g() {}, r: /c/ }; }");

