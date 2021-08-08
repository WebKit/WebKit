// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of parseFloat does not have the attribute DontDelete
esid: sec-parsefloat-string
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (parseFloat.hasOwnProperty('length') !== true) {
  throw new Test262Error('#1: parseFloat.hasOwnProperty(\'length\') === true. Actual: ' + (parseFloat.hasOwnProperty('length')));
}

delete parseFloat.length;

//CHECK#2
if (parseFloat.hasOwnProperty('length') !== false) {
  throw new Test262Error('#2: delete parseFloat.length; parseFloat.hasOwnProperty(\'length\') === false. Actual: ' + (parseFloat.hasOwnProperty('length')));
}

//CHECK#3
if (parseFloat.length === undefined) {
  throw new Test262Error('#3: delete parseFloat.length; parseFloat.length !== undefined');
}
