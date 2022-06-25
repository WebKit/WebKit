// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Step 4 of defineProperty calls the [[DefineOwnProperty]] internal method
    of O to define the property. Step 6 of [[DefineOwnProperty]] returns if
    every field of desc also occurs in current and every field in desc has
    the same value as current.
es5id: 15.2.3.6-4-6
description: >
    Object.defineProperty is no-op if current and desc are the same
    accessor desc
---*/

function sameAccessorDescriptorValues(d1, d2) {
  return (d1.get == d2.get &&
    d1.enumerable == d2.enumerable &&
    d1.configurable == d2.configurable);
}

var o = {};

// create an accessor property with the following attributes:
// enumerable: true, configurable: true
var desc = {
  get: function() {},
  enumerable: true,
  configurable: true
};

Object.defineProperty(o, "foo", desc);

// query for, and save, the desc. A subsequent call to defineProperty
// with the same desc should not disturb the property definition.
var d1 = Object.getOwnPropertyDescriptor(o, "foo");

// now, redefine the property with the same descriptor
// the property defintion should not get disturbed.
Object.defineProperty(o, "foo", desc);

var d2 = Object.getOwnPropertyDescriptor(o, "foo");

assert.sameValue(sameAccessorDescriptorValues(d1, d2), true, 'sameAccessorDescriptorValues(d1, d2)');
