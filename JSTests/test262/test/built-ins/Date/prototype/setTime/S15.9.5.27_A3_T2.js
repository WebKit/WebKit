// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setTime property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setTime.length !== true) {
  throw new Test262Error('#1: The Date.prototype.setTime.length property does not have the attributes DontDelete');
}

if (Date.prototype.setTime.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.setTime.length property does not have the attributes DontDelete');
}
