// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.valueOf property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.valueof
description: Checking DontDelete attribute
---*/

assert.sameValue(
  delete Date.prototype.valueOf.length,
  true,
  'The value of `delete Date.prototype.valueOf.length` is expected to be true'
);

assert(
  !Date.prototype.valueOf.hasOwnProperty('length'),
  'The value of !Date.prototype.valueOf.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
