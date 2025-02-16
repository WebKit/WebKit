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
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Author: Emilio Cobos Álvarez <ecoal95@gmail.com>
 */
var BUGNUMBER = 1310744;
var summary = "Dense array properties shouldn't be modified when they're frozen";

print(BUGNUMBER + ": " + summary);

var a = Object.freeze([4, 5, 1]);

function assertArrayIsExpected() {
  assert.sameValue(a.length, 3);
  assert.sameValue(a[0], 4);
  assert.sameValue(a[1], 5);
  assert.sameValue(a[2], 1);
}

assertThrowsInstanceOf(() => a.reverse(), TypeError);
assertThrowsInstanceOf(() => a.shift(), TypeError);
assertThrowsInstanceOf(() => a.unshift(0), TypeError);
assertThrowsInstanceOf(() => a.sort(function() {}), TypeError);
assertThrowsInstanceOf(() => a.pop(), TypeError);
assertThrowsInstanceOf(() => a.fill(0), TypeError);
assertThrowsInstanceOf(() => a.splice(0, 1, 1), TypeError);
assertThrowsInstanceOf(() => a.push("foo"), TypeError);
assertThrowsInstanceOf(() => { "use strict"; a.length = 5; }, TypeError);
assertThrowsInstanceOf(() => { "use strict"; a[2] = "foo"; }, TypeError);
assertThrowsInstanceOf(() => { "use strict"; delete a[0]; }, TypeError);
assertThrowsInstanceOf(() => a.splice(Math.a), TypeError);

// Shouldn't throw, since this is not strict mode, but shouldn't change the
// value of the property.
a.length = 5;
a[2] = "foo";
assert.sameValue(delete a[0], false);

assertArrayIsExpected();

