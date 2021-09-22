// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: |
    The Date.prototype.getDay property "length" has { ReadOnly, ! DontDelete,
    DontEnum } attributes
es5id: 15.9.5.16_A3_T2
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getDay.length,
  true,
  'The value of `delete Date.prototype.getDay.length` is expected to be true'
);

assert(
  !Date.prototype.getDay.hasOwnProperty('length'),
  'The value of !Date.prototype.getDay.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
