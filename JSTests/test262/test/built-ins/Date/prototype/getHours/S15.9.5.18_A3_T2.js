// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: |
    The Date.prototype.getHours property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.18_A3_T2
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getHours.length,
  true,
  'The value of `delete Date.prototype.getHours.length` is expected to be true'
);

assert(
  !Date.prototype.getHours.hasOwnProperty('length'),
  'The value of !Date.prototype.getHours.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
