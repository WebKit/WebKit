// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: |
    The Date.prototype.getFullYear property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.10_A3_T2
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getFullYear.length,
  true,
  'The value of `delete Date.prototype.getFullYear.length` is expected to be true'
);

assert(
  !Date.prototype.getFullYear.hasOwnProperty('length'),
  'The value of !Date.prototype.getFullYear.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
