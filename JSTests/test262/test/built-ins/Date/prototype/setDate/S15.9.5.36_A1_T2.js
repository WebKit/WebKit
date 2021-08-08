// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setDate" has { DontEnum } attributes
esid: sec-date.prototype.setdate
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setDate === false) {
  throw new Test262Error('#1: The Date.prototype.setDate property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setDate')) {
  throw new Test262Error('#2: The Date.prototype.setDate property has not the attributes DontDelete');
}
