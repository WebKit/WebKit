// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date property "parse" has { DontEnum } attributes
esid: sec-date.parse
description: Checking absence of ReadOnly attribute
---*/

var x = Date.parse;
if (x === 1) {
  Date.parse = 2;
} else {
  Date.parse = 1;
}
assert.notSameValue(Date.parse, x, 'The value of Date.parse is expected to not equal the value of `x`');

// TODO: Convert to verifyProperty() format.
