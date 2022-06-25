// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getDate property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getDate.length,
  true,
  'The value of `delete Date.prototype.getDate.length` is expected to be true'
);

assert(
  !Date.prototype.getDate.hasOwnProperty('length'),
  'The value of !Date.prototype.getDate.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
