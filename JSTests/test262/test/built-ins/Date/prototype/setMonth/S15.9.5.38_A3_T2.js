// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setMonth.length,
  true,
  'The value of `delete Date.prototype.setMonth.length` is expected to be true'
);

assert(
  !Date.prototype.setMonth.hasOwnProperty('length'),
  'The value of !Date.prototype.setMonth.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
