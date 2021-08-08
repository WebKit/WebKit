// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: |
    The Date.UTC property "length" has { ReadOnly, ! DontDelete, DontEnum }
    attributes
es5id: 15.9.4.3_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.UTC.length !== true) {
  throw new Test262Error('#1: The Date.UTC.length property does not have the attributes DontDelete');
}

if (Date.UTC.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.UTC.length property does not have the attributes DontDelete');
}
