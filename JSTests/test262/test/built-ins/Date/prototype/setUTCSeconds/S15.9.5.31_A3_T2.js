// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setUTCSeconds.length !== true) {
  throw new Test262Error('#1: The Date.prototype.setUTCSeconds.length property does not have the attributes DontDelete');
}

if (Date.prototype.setUTCSeconds.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.setUTCSeconds.length property does not have the attributes DontDelete');
}
