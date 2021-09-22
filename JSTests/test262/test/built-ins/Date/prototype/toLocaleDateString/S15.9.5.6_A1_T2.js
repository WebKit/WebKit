// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleDateString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaledatestring
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.toLocaleDateString,
  false,
  'The value of delete Date.prototype.toLocaleDateString is not false'
);

assert(
  !Date.prototype.hasOwnProperty('toLocaleDateString'),
  'The value of !Date.prototype.hasOwnProperty(\'toLocaleDateString\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
