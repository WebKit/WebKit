// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setTime" has { DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setTime === false) {
  $ERROR('#1: The Date.prototype.setTime property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setTime')) {
  $ERROR('#2: The Date.prototype.setTime property has not the attributes DontDelete');
}
