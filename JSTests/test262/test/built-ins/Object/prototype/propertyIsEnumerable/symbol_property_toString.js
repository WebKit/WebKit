// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Object.prototype.propertyIsEnumerable with symbol and toString conversion
info: |
  19.1.3.4 Object.prototype.propertyIsEnumerable ( V )

  1. Let P be ToPropertyKey(V).
  2. ReturnIfAbrupt(P).
  ...
es6id: 19.1.3.4
features: [Symbol]
---*/

var obj = {};
var sym = Symbol();

var callCount = 0;
var wrapper = {
  toString: function() {
    callCount += 1;
    return sym;
  },
  valueOf: function() {
    throw new Test262Error("valueOf() called");
  }
};

obj[sym] = 0;

assert.sameValue(
  obj.propertyIsEnumerable(wrapper),
  true,
  "Returns true if symbol own enumerable property found"
);

assert.sameValue(callCount, 1, "toString method called exactly once");
