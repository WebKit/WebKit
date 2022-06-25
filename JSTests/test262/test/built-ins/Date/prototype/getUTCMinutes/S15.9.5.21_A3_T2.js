// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMinutes property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getUTCMinutes.length,
  true,
  'The value of `delete Date.prototype.getUTCMinutes.length` is expected to be true'
);

assert(
  !Date.prototype.getUTCMinutes.hasOwnProperty('length'),
  'The value of !Date.prototype.getUTCMinutes.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
