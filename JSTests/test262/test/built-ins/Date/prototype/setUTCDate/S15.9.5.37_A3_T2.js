// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCDate property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setUTCDate.length,
  true,
  'The value of `delete Date.prototype.setUTCDate.length` is expected to be true'
);

assert(
  !Date.prototype.setUTCDate.hasOwnProperty('length'),
  'The value of !Date.prototype.setUTCDate.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
