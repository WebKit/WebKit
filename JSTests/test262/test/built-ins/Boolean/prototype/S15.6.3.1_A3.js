// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Boolean.prototype has the attribute DontDelete
esid: sec-boolean.prototype
description: Checking if deleting the Boolean.prototype property fails
includes: [propertyHelper.js]
---*/

// CHECK#1
verifyNotConfigurable(Boolean, "prototype");

try {
  if (delete Boolean.prototype !== false) {
    throw new Test262Error('#1: Boolean.prototype has the attribute DontDelete');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
