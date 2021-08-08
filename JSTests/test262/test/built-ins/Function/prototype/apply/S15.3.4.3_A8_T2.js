// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Function.prototype.apply can`t be used as [[Construct]] caller
es5id: 15.3.4.3_A8_T2
description: Checking if creating "new Function.prototype.apply()" fails
---*/

try {
  var obj = new Function.prototype.apply();
  throw new Test262Error('#1: Function.prototype.apply can\'t be used as [[Construct]] caller');
} catch (e) {
  if (!(e instanceof TypeError)) {
    throw new Test262Error('#1.1: Function.prototype.apply can\'t be used as [[Construct]] caller');
  }
}
