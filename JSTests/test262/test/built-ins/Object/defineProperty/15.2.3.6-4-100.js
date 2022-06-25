// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-100
description: >
    Object.defineProperty - 'name' and 'desc' are data properties,
    desc.value and name.value are two different values (8.12.9 step 12)
includes: [propertyHelper.js]
---*/

var obj = {};

obj.foo = 100; // default value of attributes: writable: true, configurable: true, enumerable: true

Object.defineProperty(obj, "foo", {
  value: 200
});
verifyEqualTo(obj, "foo", 200);

verifyWritable(obj, "foo");

verifyEnumerable(obj, "foo");

verifyConfigurable(obj, "foo");
