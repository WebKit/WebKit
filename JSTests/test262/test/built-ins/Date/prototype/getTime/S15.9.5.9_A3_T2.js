// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getTime property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getTime.length,
  true,
  'The value of `delete Date.prototype.getTime.length` is expected to be true'
);

assert(
  !Date.prototype.getTime.hasOwnProperty('length'),
  'The value of !Date.prototype.getTime.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
