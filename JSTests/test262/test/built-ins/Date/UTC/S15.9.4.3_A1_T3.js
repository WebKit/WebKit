// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: The Date property "UTC" has { DontEnum } attributes
es5id: 15.9.4.3_A1_T3
description: Checking DontEnum attribute
---*/

if (Date.propertyIsEnumerable('UTC')) {
  throw new Test262Error('#1: The Date.UTC property has the attribute DontEnum');
}

for (var x in Date) {
  if (x === "UTC") {
    throw new Test262Error('#2: The Date.UTC has the attribute DontEnum');
  }
}
