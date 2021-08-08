// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "getTimezoneOffset" has { DontEnum }
    attributes
esid: sec-date.prototype.gettimezoneoffset
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getTimezoneOffset')) {
  throw new Test262Error('#1: The Date.prototype.getTimezoneOffset property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getTimezoneOffset") {
    throw new Test262Error('#2: The Date.prototype.getTimezoneOffset has the attribute DontEnum');
  }
}
