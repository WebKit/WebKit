// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setDate property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setdate
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setDate.length,
  true,
  'The value of `delete Date.prototype.setDate.length` is expected to be true'
);

assert(
  !Date.prototype.setDate.hasOwnProperty('length'),
  'The value of !Date.prototype.setDate.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
