// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setHours" has { DontEnum } attributes
esid: sec-date.prototype.sethours
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setHours === false) {
  $ERROR('#1: The Date.prototype.setHours property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setHours')) {
  $ERROR('#2: The Date.prototype.setHours property has not the attributes DontDelete');
}
