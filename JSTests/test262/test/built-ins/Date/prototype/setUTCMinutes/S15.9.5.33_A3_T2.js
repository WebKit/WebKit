// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMinutes property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setUTCMinutes.length,
  true,
  'The value of `delete Date.prototype.setUTCMinutes.length` is expected to be true'
);

assert(
  !Date.prototype.setUTCMinutes.hasOwnProperty('length'),
  'The value of !Date.prototype.setUTCMinutes.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
