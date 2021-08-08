// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.9-2-c-1
description: >
    Object.freeze - The [[Configurable]] attribute of own data
    property of 'O' is set to false while other attributes are
    unchanged
includes: [propertyHelper.js]
---*/

var obj = {};

Object.defineProperty(obj, "foo", {
  value: 10,
  writable: false,
  enumerable: true,
  configurable: true
});

Object.freeze(obj);

var desc = Object.getOwnPropertyDescriptor(obj, "foo");

if (desc.configurable !== false) {
  throw new Test262Error("Expected desc.configurable to be false, actually " + desc.configurable);
}
if (desc.writable !== false) {
  throw new Test262Error("Expected desc.writable to be false, actually " + desc.writable);
}

verifyEqualTo(obj, "foo", 10);

verifyNotWritable(obj, "foo");

verifyEnumerable(obj, "foo");

verifyNotConfigurable(obj, "foo");
