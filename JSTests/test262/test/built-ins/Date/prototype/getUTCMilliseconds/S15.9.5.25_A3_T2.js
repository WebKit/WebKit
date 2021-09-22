// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMilliseconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcmilliseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getUTCMilliseconds.length,
  true,
  'The value of `delete Date.prototype.getUTCMilliseconds.length` is expected to be true'
);

assert(
  !Date.prototype.getUTCMilliseconds.hasOwnProperty('length'),
  'The value of !Date.prototype.getUTCMilliseconds.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
