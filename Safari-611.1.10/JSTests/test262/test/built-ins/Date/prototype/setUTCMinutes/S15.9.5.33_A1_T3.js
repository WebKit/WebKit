// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('setUTCMinutes')) {
  $ERROR('#1: The Date.prototype.setUTCMinutes property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "setUTCMinutes") {
    $ERROR('#2: The Date.prototype.setUTCMinutes has the attribute DontEnum');
  }
}
