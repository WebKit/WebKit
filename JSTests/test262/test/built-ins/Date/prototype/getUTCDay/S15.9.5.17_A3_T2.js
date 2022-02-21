// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCDay property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcdaty
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getUTCDay.length,
  true,
  'The value of `delete Date.prototype.getUTCDay.length` is expected to be true'
);

assert(
  !Date.prototype.getUTCDay.hasOwnProperty('length'),
  'The value of !Date.prototype.getUTCDay.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
