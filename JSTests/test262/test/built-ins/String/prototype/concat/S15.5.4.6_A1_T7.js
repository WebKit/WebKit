// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.concat([,[...]])
es5id: 15.5.4.6_A1_T7
description: >
    Call concat([,[...]]) function with undefined argument of string
    object
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
//since ToString(undefined) evaluates to "undefined" concat(undefined) evaluates to concat("undefined")
if (String("lego").concat(undefined) !== "legoundefined") {
  throw new Test262Error('#1: String("lego").concat(undefined) === "legoundefined". Actual: ' + String("lego").concat(undefined));
}
//
//////////////////////////////////////////////////////////////////////////////
