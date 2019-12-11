// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getUTCFullYear === false) {
  $ERROR('#1: The Date.prototype.getUTCFullYear property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getUTCFullYear')) {
  $ERROR('#2: The Date.prototype.getUTCFullYear property has not the attributes DontDelete');
}
