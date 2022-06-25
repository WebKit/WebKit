// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Prototype]] property of the newly constructed object is set to the
    original String prototype object
es5id: 15.5.2.1_A2_T2
description: >
    Creating string object with "new String(string)" adding custom
    property
---*/

var __str__obj = new String("shocking blue");

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__str__obj["__custom__prop"] !== undefined) {
  throw new Test262Error('#1: var __str__obj = new String("shocking blue"); __str__obj["__custom__prop"]===undefined. Actual: __str__obj["__custom__prop"]===' + __str__obj["__custom__prop"]);
}
//
//////////////////////////////////////////////////////////////////////////////

String.prototype.__custom__prop = "bor";

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (__str__obj["__custom__prop"] !== "bor") {
  throw new Test262Error('#2: var __str__obj = new String("shocking blue"); String.prototype.__custom__prop = "bor"; __str__obj["__custom__prop"]==="bor". Actual: __str__obj["__custom__prop"]===' + __str__obj["__custom__prop"]);
}
//
//////////////////////////////////////////////////////////////////////////////
