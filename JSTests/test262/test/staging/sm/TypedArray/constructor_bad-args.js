// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Bug 1227207

var AB = new ArrayBuffer(12);	// Length divides 4
var BC = new ArrayBuffer(14);	// Length does not divide 4

assertThrowsInstanceOf(() => new Int32Array(AB, -1), RangeError); // 22.2.4.5 #8
assertThrowsInstanceOf(() => new Int32Array(AB, 2), RangeError);  // 22.2.4.5 #10
assertThrowsInstanceOf(() => new Int32Array(BC), RangeError);	  // 22.2.4.5 #13.a
assertThrowsInstanceOf(() => new Int32Array(AB, 16), RangeError); // 22.2.4.5 #13.c
assertThrowsInstanceOf(() => new Int32Array(AB, 0, 4), RangeError); // 22.2.4.5 #14.c

