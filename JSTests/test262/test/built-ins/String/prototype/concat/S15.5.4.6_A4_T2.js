// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    when String.prototype.concat([,[...]]) is called first Call ToString,
    giving it the this value as its argument
es5id: 15.5.4.6_A4_T2
description: Override toString function onto function, that throw exception
---*/

var __instance = {
  toString: function() {
    throw "intostring";
  }
};
var __obj = {
  toString: function() {
    throw "infirstarg";
  }
};

__instance.concat = String.prototype.concat;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
try {
  String.prototype.concat.call(__instance, __obj, notexist);
  throw new Test262Error('#1: "String.prototype.concat.call(__instance,__obj, notexist)" lead to throwing exception');
} catch (e) {
  if (e !== "intostring") {
    throw new Test262Error('#1: e === "intostring". Actual: ' + e);
  }
}
//
//////////////////////////////////////////////////////////////////////////////

var notexist;
