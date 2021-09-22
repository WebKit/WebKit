// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMilliseconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setMilliseconds.length,
  true,
  'The value of `delete Date.prototype.setMilliseconds.length` is expected to be true'
);

assert(
  !Date.prototype.setMilliseconds.hasOwnProperty('length'),
  'The value of !Date.prototype.setMilliseconds.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
