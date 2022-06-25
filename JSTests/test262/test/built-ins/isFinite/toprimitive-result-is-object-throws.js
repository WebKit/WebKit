// Copyright (C) 2016 The V8 Project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-isfinite-number
description: >
  Throws a TypeError if the result of calling number.@@toPrimitive is an Object
info: |
  isFinite (number)

  1. Let num be ? ToNumber(number).

  ToPrimitive ( input [ , PreferredType ] )

  [...]
  4. Let exoticToPrim be ? GetMethod(input, @@toPrimitive).
  5. If exoticToPrim is not undefined, then
    a. Let result be ? Call(exoticToPrim, input, « hint »).
    b. If Type(result) is not Object, return result.
    c. Throw a TypeError exception.
features: [Symbol.toPrimitive]
---*/

var obj = {};
obj[Symbol.toPrimitive] = function() {
  return [42];
};

assert.throws(TypeError, function() {
  isFinite(obj);
});
