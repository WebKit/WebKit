// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCDate property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcdate
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getUTCDate.length !== true) {
  $ERROR('#1: The Date.prototype.getUTCDate.length property does not have the attributes DontDelete');
}

if (Date.prototype.getUTCDate.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.getUTCDate.length property does not have the attributes DontDelete');
}
