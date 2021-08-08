// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "valueOf" has { DontEnum } attributes
esid: sec-date.prototype.valueof
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.valueOf === false) {
  throw new Test262Error('#1: The Date.prototype.valueOf property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('valueOf')) {
  throw new Test262Error('#2: The Date.prototype.valueOf property has not the attributes DontDelete');
}
