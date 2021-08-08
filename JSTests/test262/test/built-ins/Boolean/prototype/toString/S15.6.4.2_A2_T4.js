// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-boolean.prototype.tostring
info: |
    The toString function is not generic, it cannot be transferred
    to other kinds of objects for use as a method and there is should be
    a TypeError exception if its this value is not a Boolean object
es5id: 15.6.4.2_A2_T4
description: transferring to the Object objects
---*/

//CHECK#1
try {
  var s1 = new Object();
  s1.toString = Boolean.prototype.toString;
  var v1 = s1.toString();
  throw new Test262Error('#1: Boolean.prototype.toString on not a Boolean object should throw TypeError');
}
catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Test262Error('#1: Boolean.prototype.toString on not a Boolean object should throw TypeError, not ' + e);
  }
}

//CHECK#1
try {
  var s2 = new Object();
  s2.myToString = Boolean.prototype.toString;
  var v2 = s2.myToString();
  throw new Test262Error('#2: Boolean.prototype.toString on not a Boolean object should throw TypeError');
}
catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Test262Error('#2: Boolean.prototype.toString on not a Boolean object should throw TypeError, not ' + e);
  }
}
