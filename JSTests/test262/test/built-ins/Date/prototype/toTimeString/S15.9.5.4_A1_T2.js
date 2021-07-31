// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toTimeString" has { DontEnum } attributes
esid: sec-date.prototype.totimestring
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.toTimeString === false) {
  throw new Test262Error('#1: The Date.prototype.toTimeString property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('toTimeString')) {
  throw new Test262Error('#2: The Date.prototype.toTimeString property has not the attributes DontDelete');
}
