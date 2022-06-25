// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-5-b-209
description: >
    Object.defineProperties - 'descObj' is the Math object which
    implements its own [[Get]] method to get 'get' property (8.10.5
    step 7.a)
---*/

var obj = {};

Math.get = function() {
  return "Math";
};

Object.defineProperties(obj, {
  property: Math
});

assert.sameValue(obj.property, "Math", 'obj.property');
