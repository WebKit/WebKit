// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-78
description: >
    Object.defineProperties will not throw TypeError when
    P.configurable is false, P.writalbe is false, properties.value and
    P.value are two numbers with the same value (8.12.9 step 10.a.ii.1)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", {
  value: 100,
  writable: false,
  configurable: false
});

Object.defineProperties(obj, {
  foo: {
    value: 100
  }
});
verifyEqualTo(obj, "foo", 100);

verifyNotWritable(obj, "foo");

verifyNotEnumerable(obj, "foo");

verifyNotConfigurable(obj, "foo");
