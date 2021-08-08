// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "setUTCMilliseconds" has { DontEnum }
    attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('setUTCMilliseconds')) {
  throw new Test262Error('#1: The Date.prototype.setUTCMilliseconds property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "setUTCMilliseconds") {
    throw new Test262Error('#2: The Date.prototype.setUTCMilliseconds has the attribute DontEnum');
  }
}
