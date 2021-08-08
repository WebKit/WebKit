// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The String.prototype.toLowerCase.length property has the attribute
    DontEnum
es5id: 15.5.4.16_A8
description: >
    Checking if enumerating the String.prototype.toLowerCase.length
    property fails
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#0
if (!(String.prototype.toLowerCase.hasOwnProperty('length'))) {
  throw new Test262Error('#0: String.prototype.toLowerCase.hasOwnProperty(\'length\') return true. Actual: ' + String.prototype.toLowerCase.hasOwnProperty('length'));
}
//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// CHECK#1
if (String.prototype.toLowerCase.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: String.prototype.toLowerCase.propertyIsEnumerable(\'length\') return false');
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// CHECK#2
var count = 0;

for (var p in String.prototype.toLowerCase) {
  if (p === "length") count++;
}

if (count !== 0) {
  throw new Test262Error('#2: count=0; for (p in String.prototype.toLowerCase){if (p==="length") count++;}; count === 0. Actual: ' + count);
}
//
//////////////////////////////////////////////////////////////////////////////
