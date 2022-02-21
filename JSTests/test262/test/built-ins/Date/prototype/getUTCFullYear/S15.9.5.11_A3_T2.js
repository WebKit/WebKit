// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCFullYear property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getUTCFullYear.length,
  true,
  'The value of `delete Date.prototype.getUTCFullYear.length` is expected to be true'
);

assert(
  !Date.prototype.getUTCFullYear.hasOwnProperty('length'),
  'The value of !Date.prototype.getUTCFullYear.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
