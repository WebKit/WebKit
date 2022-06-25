// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getUTCMonth.length,
  true,
  'The value of `delete Date.prototype.getUTCMonth.length` is expected to be true'
);

assert(
  !Date.prototype.getUTCMonth.hasOwnProperty('length'),
  'The value of !Date.prototype.getUTCMonth.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
