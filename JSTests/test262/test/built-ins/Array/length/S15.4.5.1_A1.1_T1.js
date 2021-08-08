// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-exotic-objects-defineownproperty-p-desc
info: If ToUint32(length) !== ToNumber(length), throw RangeError
es5id: 15.4.5.1_A1.1_T1
description: length in [4294967296, -1, 1.5]
---*/

//CHECK#1
try {
  var x = [];
  x.length = 4294967296;
  throw new Test262Error('#1.1: x = []; x.length = 4294967296 throw RangeError. Actual: x.length === ' + (x.length));
} catch (e) {
  if ((e instanceof RangeError) !== true) {
    throw new Test262Error('#1.2: x = []; x.length = 4294967296 throw RangeError. Actual: ' + (e));
  }
}

//CHECK#2
try {
  x = [];
  x.length = -1;
  throw new Test262Error('#2.1: x = []; x.length = -1 throw RangeError. Actual: x.length === ' + (x.length));
} catch (e) {
  if ((e instanceof RangeError) !== true) {
    throw new Test262Error('#2.2: x = []; x.length = -1 throw RangeError. Actual: ' + (e));
  }
}

//CHECK#3
try {
  x = [];
  x.length = 1.5;
  throw new Test262Error('#3.1: x = []; x.length = 1.5 throw RangeError. Actual: x.length === ' + (x.length));
} catch (e) {
  if ((e instanceof RangeError) !== true) {
    throw new Test262Error('#3.2: x = []; x.length = 1.5 throw RangeError. Actual: ' + (e));
  }
}
