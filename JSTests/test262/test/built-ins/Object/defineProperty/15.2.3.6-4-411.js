// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-411
description: >
    ES5 Attributes - Inherited property whose [[Enumerable]] attribute
    is set to false is non-enumerable (Math)
---*/

Object.defineProperty(Object.prototype, "prop", {
  value: 1001,
  writable: false,
  enumerable: false,
  configurable: true
});

var verifyEnumerable = false;
for (var p in Math) {
  if (p === "prop") {
    verifyEnumerable = true;
  }
}

assert.sameValue(Math.hasOwnProperty("prop"), false, 'Math.hasOwnProperty("prop")');
assert.sameValue(verifyEnumerable, false, 'verifyEnumerable');
