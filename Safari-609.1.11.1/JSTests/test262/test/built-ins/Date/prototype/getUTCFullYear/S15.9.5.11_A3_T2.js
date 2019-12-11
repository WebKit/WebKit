// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCFullYear property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getUTCFullYear.length !== true) {
  $ERROR('#1: The Date.prototype.getUTCFullYear.length property does not have the attributes DontDelete');
}

if (Date.prototype.getUTCFullYear.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.getUTCFullYear.length property does not have the attributes DontDelete');
}
