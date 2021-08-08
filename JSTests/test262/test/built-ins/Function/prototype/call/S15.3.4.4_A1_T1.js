// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The call method performs a function call using the [[Call]] property of
    the object. If the object does not have a [[Call]] property, a TypeError
    exception is thrown
es5id: 15.3.4.4_A1_T1
description: >
    Call "call" method of the object that does not have a [[Call]]
    property.  Prototype of the object is Function()
---*/

var proto = Function();

function FACTORY() {};

FACTORY.prototype = proto;

var obj = new FACTORY;

//CHECK#1
if (typeof obj.call !== "function") {
  throw new Test262Error('#1: call method accessed');
}

//CHECK#2
try {
  obj.call();
  throw new Test262Error('#2: If the object does not have a [[Call]] property, a TypeError exception is thrown');
} catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Test262Error('#2.1: If the object does not have a [[Call]] property, a TypeError exception is thrown');
  }
}
