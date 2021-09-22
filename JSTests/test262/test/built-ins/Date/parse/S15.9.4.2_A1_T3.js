// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date property "parse" has { DontEnum } attributes
esid: sec-date.parse
description: Checking DontEnum attribute
---*/
assert(
  !Date.propertyIsEnumerable('parse'),
  'The value of !Date.propertyIsEnumerable(\'parse\') is expected to be true'
);

for (var x in Date) {
  assert.notSameValue(x, "parse", 'The value of x is not "parse"');
}

// TODO: Convert to verifyProperty() format.
