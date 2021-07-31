// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the hasOwnProperty method is called with argument V, the following steps are taken:
    i) Let O be this object
    ii) Call ToString(V)
    iii) If O doesn't have a property with the name given by Result(ii), return false
    iv) Return true
es5id: 15.2.4.5_A1_T3
description: >
    Argument of the hasOwnProperty method is a custom property of a
    function object
---*/

var FACTORY = function() {
  this.aproperty = 1;
};

var instance = new FACTORY;

//CHECK#1
if (typeof Object.prototype.hasOwnProperty !== "function") {
  throw new Test262Error('#1: hasOwnProperty method is defined');
}

//CHECK#2
if (typeof instance.hasOwnProperty !== "function") {
  throw new Test262Error('#2: hasOwnProperty method is accessed');
}

//CHECK#3
if (instance.hasOwnProperty("toString")) {
  throw new Test262Error('#3: hasOwnProperty method works properly');
}

//CHECK#4
if (!(instance.hasOwnProperty("aproperty"))) {
  throw new Test262Error('#4: hasOwnProperty method works properly');
}
