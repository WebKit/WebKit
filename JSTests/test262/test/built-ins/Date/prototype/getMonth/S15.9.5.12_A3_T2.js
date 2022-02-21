// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getMonth.length,
  true,
  'The value of `delete Date.prototype.getMonth.length` is expected to be true'
);

assert(
  !Date.prototype.getMonth.hasOwnProperty('length'),
  'The value of !Date.prototype.getMonth.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
