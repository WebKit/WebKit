// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMilliseconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setUTCMilliseconds.length,
  true,
  'The value of `delete Date.prototype.setUTCMilliseconds.length` is expected to be true'
);

assert(
  !Date.prototype.setUTCMilliseconds.hasOwnProperty('length'),
  'The value of !Date.prototype.setUTCMilliseconds.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
