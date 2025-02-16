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
var actual;
var expect = "function f () { ff (); }";
function fun() {
    (new Function ("function ff () { actual = '' + ff. caller; } function f () { ff (); } f ();")) ();
}
fun();
assert.sameValue(expect, actual, "");
