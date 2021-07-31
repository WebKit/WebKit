// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-len
info: |
    If the argument len is a Number and ToUint32(len) is not equal to len,
    a RangeError exception is thrown
es5id: 15.4.2.2_A2.2_T2
description: Use try statement. len = NaN, +/-Infinity
---*/

//CHECK#1
try {
  new Array(NaN);
  throw new Test262Error('#1.1: new Array(NaN) throw RangeError. Actual: ' + (new Array(NaN)));
} catch (e) {
  if ((e instanceof RangeError) !== true) {
    throw new Test262Error('#1.2: new Array(NaN) throw RangeError. Actual: ' + (e));
  }
}

//CHECK#2
try {
  new Array(Number.POSITIVE_INFINITY);
  throw new Test262Error('#2.1: new Array(Number.POSITIVE_INFINITY) throw RangeError. Actual: ' + (new Array(Number.POSITIVE_INFINITY)));
} catch (e) {
  if ((e instanceof RangeError) !== true) {
    throw new Test262Error('#2.2: new Array(Number.POSITIVE_INFINITY) throw RangeError. Actual: ' + (e));
  }
}

//CHECK#3
try {
  new Array(Number.NEGATIVE_INFINITY);
  throw new Test262Error('#3.1: new Array(Number.NEGATIVE_INFINITY) throw RangeError. Actual: ' + (new Array(Number.NEGATIVE_INFINITY)));
} catch (e) {
  if ((e instanceof RangeError) !== true) {
    throw new Test262Error('#3.2: new Array(Number.NEGATIVE_INFINITY) throw RangeError. Actual: ' + (e));
  }
}
