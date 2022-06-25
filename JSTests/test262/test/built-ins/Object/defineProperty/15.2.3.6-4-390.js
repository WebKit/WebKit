// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-390
description: >
    ES5 Attributes - [[Value]] attribute of data property is a
    Function object
---*/

var obj = {};
var funObj = function() {};

Object.defineProperty(obj, "prop", {
  value: funObj
});

var desc = Object.getOwnPropertyDescriptor(obj, "prop");

assert.sameValue(obj.prop, funObj, 'obj.prop');
assert.sameValue(desc.value, funObj, 'desc.value');
