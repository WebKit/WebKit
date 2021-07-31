// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCDate property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setUTCDate.length !== true) {
  throw new Test262Error('#1: The Date.prototype.setUTCDate.length property does not have the attributes DontDelete');
}

if (Date.prototype.setUTCDate.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.setUTCDate.length property does not have the attributes DontDelete');
}
