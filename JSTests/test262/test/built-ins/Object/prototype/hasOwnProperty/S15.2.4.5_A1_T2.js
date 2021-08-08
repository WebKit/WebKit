// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the hasOwnProperty method is called with argument V, the following steps are taken:
    i) Let O be this object
    ii) Call ToString(V)
    iii) If O doesn't have a property with the name given by Result(ii), return false
    iv) Return true
es5id: 15.2.4.5_A1_T2
description: Argument of the hasOwnProperty method is a custom boolean property
---*/

//CHECK#1
if (typeof Object.prototype.hasOwnProperty !== "function") {
  throw new Test262Error('#1: hasOwnProperty method is defined');
}

var obj = {
  the_property: true
};

//CHECK#2
if (typeof obj.hasOwnProperty !== "function") {
  throw new Test262Error('#2: hasOwnProperty method is accessed');
}

//CHECK#3
if (obj.hasOwnProperty("hasOwnProperty")) {
  throw new Test262Error('#3: hasOwnProperty method works properly');
}

//CHECK#4
if (!(obj.hasOwnProperty("the_property"))) {
  throw new Test262Error('#4: hasOwnProperty method works properly');
}
//
