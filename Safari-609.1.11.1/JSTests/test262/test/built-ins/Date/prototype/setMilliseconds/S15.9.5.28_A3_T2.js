// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMilliseconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setMilliseconds.length !== true) {
  $ERROR('#1: The Date.prototype.setMilliseconds.length property does not have the attributes DontDelete');
}

if (Date.prototype.setMilliseconds.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.setMilliseconds.length property does not have the attributes DontDelete');
}
