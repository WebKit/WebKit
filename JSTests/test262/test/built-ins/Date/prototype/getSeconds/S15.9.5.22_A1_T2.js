// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getSeconds" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getSeconds === false) {
  throw new Test262Error('#1: The Date.prototype.getSeconds property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getSeconds')) {
  throw new Test262Error('#2: The Date.prototype.getSeconds property has not the attributes DontDelete');
}
