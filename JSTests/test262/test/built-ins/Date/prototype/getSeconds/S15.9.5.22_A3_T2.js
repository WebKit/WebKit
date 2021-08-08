// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getSeconds.length !== true) {
  throw new Test262Error('#1: The Date.prototype.getSeconds.length property does not have the attributes DontDelete');
}

if (Date.prototype.getSeconds.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.getSeconds.length property does not have the attributes DontDelete');
}
