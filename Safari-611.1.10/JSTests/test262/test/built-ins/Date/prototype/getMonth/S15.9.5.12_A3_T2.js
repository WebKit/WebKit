// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getMonth.length !== true) {
  $ERROR('#1: The Date.prototype.getMonth.length property does not have the attributes DontDelete');
}

if (Date.prototype.getMonth.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.getMonth.length property does not have the attributes DontDelete');
}
