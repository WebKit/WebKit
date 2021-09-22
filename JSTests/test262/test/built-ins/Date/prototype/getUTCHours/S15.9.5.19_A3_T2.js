// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCHours property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutchours
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getUTCHours.length,
  true,
  'The value of `delete Date.prototype.getUTCHours.length` is expected to be true'
);

assert(
  !Date.prototype.getUTCHours.hasOwnProperty('length'),
  'The value of !Date.prototype.getUTCHours.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
