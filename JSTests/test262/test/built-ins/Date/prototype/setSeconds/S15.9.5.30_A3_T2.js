// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setSeconds.length,
  true,
  'The value of `delete Date.prototype.setSeconds.length` is expected to be true'
);

assert(
  !Date.prototype.setSeconds.hasOwnProperty('length'),
  'The value of !Date.prototype.setSeconds.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
