// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleDateString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaledatestring
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.toLocaleDateString === false) {
  throw new Test262Error('#1: The Date.prototype.toLocaleDateString property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('toLocaleDateString')) {
  throw new Test262Error('#2: The Date.prototype.toLocaleDateString property has not the attributes DontDelete');
}
