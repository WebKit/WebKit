// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The apply method performs a function call using the [[Call]] property of
    the object. If the object does not have a [[Call]] property, a TypeError
    exception is thrown
es5id: 15.3.4.3_A1_T2
description: >
    Calling "apply" method of the object that does not have a [[Call]]
    property.  Prototype of the object is Function.prototype
---*/

function FACTORY() {};

FACTORY.prototype = Function.prototype;

var obj = new FACTORY;

//CHECK#1
if (typeof obj.apply !== "function") {
  throw new Test262Error('#1: apply method accessed');
}

//CHECK#2
try {
  obj.apply();
  throw new Test262Error('#2: If the object does not have a [[Call]] property, a TypeError exception is thrown');
} catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Test262Error('#2.1: If the object does not have a [[Call]] property, a TypeError exception is thrown');
  }
}
