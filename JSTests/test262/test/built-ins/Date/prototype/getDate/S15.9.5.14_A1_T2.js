// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getDate" has { DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getDate === false) {
  throw new Test262Error('#1: The Date.prototype.getDate property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getDate')) {
  throw new Test262Error('#2: The Date.prototype.getDate property has not the attributes DontDelete');
}
