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

// Private names aren't valid in object literals.

assertThrowsInstanceOf(() => eval(`var o = {#a: 0};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {#a};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {#a(){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {get #a(){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {set #a(v){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {*#a(v){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {async #a(v){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {async *#a(v){}};`), SyntaxError);

