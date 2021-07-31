// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getTime" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getTime')) {
  throw new Test262Error('#1: The Date.prototype.getTime property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getTime") {
    throw new Test262Error('#2: The Date.prototype.getTime has the attribute DontEnum');
  }
}
