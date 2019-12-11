// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMinutes property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setUTCMinutes.length !== true) {
  $ERROR('#1: The Date.prototype.setUTCMinutes.length property does not have the attributes DontDelete');
}

if (Date.prototype.setUTCMinutes.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.setUTCMinutes.length property does not have the attributes DontDelete');
}
