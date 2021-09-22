// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: The Date property "UTC" has { DontEnum } attributes
es5id: 15.9.4.3_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.UTC;
if (x === 1) {
  Date.UTC = 2;
} else {
  Date.UTC = 1;
}
assert.notSameValue(Date.UTC, x, 'The value of Date.UTC is expected to not equal the value of `x`');

// TODO: Convert to verifyProperty() format.
