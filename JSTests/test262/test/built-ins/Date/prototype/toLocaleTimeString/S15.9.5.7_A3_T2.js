// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleTimeString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.toLocaleTimeString.length !== true) {
  throw new Test262Error('#1: The Date.prototype.toLocaleTimeString.length property does not have the attributes DontDelete');
}

if (Date.prototype.toLocaleTimeString.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.toLocaleTimeString.length property does not have the attributes DontDelete');
}
