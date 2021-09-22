// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setTime property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setTime.length,
  true,
  'The value of `delete Date.prototype.setTime.length` is expected to be true'
);

assert(
  !Date.prototype.setTime.hasOwnProperty('length'),
  'The value of !Date.prototype.setTime.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
