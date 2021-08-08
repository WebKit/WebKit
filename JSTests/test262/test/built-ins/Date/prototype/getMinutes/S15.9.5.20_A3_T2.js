// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: |
    The Date.prototype.getMinutes property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.20_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getMinutes.length !== true) {
  throw new Test262Error('#1: The Date.prototype.getMinutes.length property does not have the attributes DontDelete');
}

if (Date.prototype.getMinutes.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.getMinutes.length property does not have the attributes DontDelete');
}
