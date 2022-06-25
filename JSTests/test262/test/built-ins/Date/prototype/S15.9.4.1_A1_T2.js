// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date property "prototype" has { DontEnum, DontDelete, ReadOnly }
    attributes
esid: sec-date.prototype
description: Checking DontDelete attribute
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Date, "prototype");

try {
  assert.sameValue(delete Date.prototype, false);
} catch (e) {
  if (e instanceof Test262Error) {
    throw e;
  }
  assert(e instanceof TypeError);
}

if (!Date.hasOwnProperty('prototype')) {
  throw new Test262Error('#2: The Date.prototype property has the attributes DontDelete');
}

// TODO: Convert to verifyProperty() format.
