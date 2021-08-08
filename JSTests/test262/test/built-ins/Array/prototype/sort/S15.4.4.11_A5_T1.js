// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Array.sort should not eat exceptions
esid: sec-array.prototype.sort
description: comparefn function throw "error"
---*/

//CHECK#1
var myComparefn = function(x, y) {
  throw "error";
}
var x = [1, 0];
try {
  x.sort(myComparefn)
  throw new Test262Error('#1.1: Array.sort should not eat exceptions');
} catch (e) {
  if (e !== "error") {
    throw new Test262Error('#1.2: Array.sort should not eat exceptions');
  }
}
