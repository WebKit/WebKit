// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcmonth
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setUTCMonth.length,
  true,
  'The value of `delete Date.prototype.setUTCMonth.length` is expected to be true'
);

assert(
  !Date.prototype.setUTCMonth.hasOwnProperty('length'),
  'The value of !Date.prototype.setUTCMonth.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
