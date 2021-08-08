// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.MIN_VALUE is DontDelete
es5id: 15.7.3.3_A3
description: Checking if deleting Number.MIN_VALUE fails
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Number, "MIN_VALUE");

//CHECK#1
try {
  if (delete Number.MIN_VALUE !== false) {
    throw new Test262Error('#1: delete Number.MIN_VALUE === false');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
