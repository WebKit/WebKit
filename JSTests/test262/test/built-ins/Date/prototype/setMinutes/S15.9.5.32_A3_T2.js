// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMinutes property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setMinutes.length,
  true,
  'The value of `delete Date.prototype.setMinutes.length` is expected to be true'
);

assert(
  !Date.prototype.setMinutes.hasOwnProperty('length'),
  'The value of !Date.prototype.setMinutes.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
