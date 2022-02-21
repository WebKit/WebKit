// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toDateString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.todatestring
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.toDateString.length,
  true,
  'The value of `delete Date.prototype.toDateString.length` is expected to be true'
);

assert(
  !Date.prototype.toDateString.hasOwnProperty('length'),
  'The value of !Date.prototype.toDateString.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
