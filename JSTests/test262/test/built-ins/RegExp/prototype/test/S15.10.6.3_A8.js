// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype.test.length property has the attribute DontEnum
es5id: 15.10.6.3_A8
description: >
    Checking if enumerating the RegExp.prototype.test.length property
    fails
---*/

//CHECK#0
if (RegExp.prototype.test.hasOwnProperty('length') !== true) {
  throw new Test262Error('#0: RegExp.prototype.test.hasOwnProperty(\'length\') === true');
}

 //CHECK#1
if (RegExp.prototype.test.propertyIsEnumerable('length') !== false) {
  throw new Test262Error('#1: RegExp.prototype.test.propertyIsEnumerable(\'length\') === true');
}

 //CHECK#2
var count=0;

for (var p in RegExp.prototype.test){
  if (p==="length") count++;
}

if (count !== 0) {
  throw new Test262Error('#2: count = 0; for (p in RegExp.prototype.test){ if (p==="length") count++; } count === 0. Actual: ' + (count));
}
