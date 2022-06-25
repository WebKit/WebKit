// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-452
description: >
    ES5 Attributes - property ([[Get]] is undefined, [[Set]] is
    undefined, [[Enumerable]] is false, [[Configurable]] is false) is
    undeletable
includes: [propertyHelper.js]
---*/

var obj = {};

Object.defineProperty(obj, "prop", {
  get: undefined,
  set: undefined,
  enumerable: false,
  configurable: false
});

assert(obj.hasOwnProperty("prop"));

var desc = Object.getOwnPropertyDescriptor(obj, "prop");

assert.sameValue(desc.configurable, false);

verifyNotConfigurable(obj, "prop");
