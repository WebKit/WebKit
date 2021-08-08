// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The propertyIsEnumerable method does not consider objects in the
    prototype chain
es5id: 15.2.4.7_A1_T1
description: >
    Calling the propertyIsEnumerable method for object in the
    prototype chain
---*/

//CHECK#1
if (typeof Object.prototype.propertyIsEnumerable !== "function") {
  throw new Test262Error('#1: propertyIsEnumerable method is defined');
}

var proto = {
  rootprop: "avis"
};

function AVISFACTORY(name) {
  this.name = name
};

AVISFACTORY.prototype = proto;

var seagull = new AVISFACTORY("seagull");

//CHECK#2
if (typeof seagull.propertyIsEnumerable !== "function") {
  throw new Test262Error('#2: propertyIsEnumerable method is accessed');
}

//CHECK#3
if (!(seagull.propertyIsEnumerable("name"))) {
  throw new Test262Error('#3: propertyIsEnumerable method works properly');
}

//CHECK#4
if (seagull.propertyIsEnumerable("rootprop")) {
  throw new Test262Error('#4: propertyIsEnumerable method does not consider objects in the prototype chain');
}
//
