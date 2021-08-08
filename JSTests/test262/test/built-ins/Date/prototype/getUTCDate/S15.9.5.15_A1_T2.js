// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCDate" has { DontEnum } attributes
esid: sec-date.prototype.getutcdate
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getUTCDate === false) {
  throw new Test262Error('#1: The Date.prototype.getUTCDate property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getUTCDate')) {
  throw new Test262Error('#2: The Date.prototype.getUTCDate property has not the attributes DontDelete');
}
