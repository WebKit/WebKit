// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMilliseconds" has { DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setMilliseconds === false) {
  throw new Test262Error('#1: The Date.prototype.setMilliseconds property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setMilliseconds')) {
  throw new Test262Error('#2: The Date.prototype.setMilliseconds property has not the attributes DontDelete');
}
