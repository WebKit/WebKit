// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getSeconds.length,
  true,
  'The value of `delete Date.prototype.getSeconds.length` is expected to be true'
);

assert(
  !Date.prototype.getSeconds.hasOwnProperty('length'),
  'The value of !Date.prototype.getSeconds.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
