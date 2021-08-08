// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The value of the internal [[Prototype]] property of
    the Array constructor is the Function prototype object
es5id: 15.4.3_A1.1_T1
description: >
    Create new property of Function.prototype. When Array constructor
    has this property
---*/

Function.prototype.myproperty = 1;

//CHECK#1
if (Array.myproperty !== 1) {
  throw new Test262Error('#1: Function.prototype.myproperty = 1; Array.myproperty === 1. Actual: ' + (Array.myproperty));
}

//CHECK#2
if (Array.hasOwnProperty('myproperty') !== false) {
  throw new Test262Error('#2: Function.prototype.myproperty = 1; Array.hasOwnProperty(\'myproperty\') === false. Actual: ' + (Array.hasOwnProperty('myproperty')));
}
