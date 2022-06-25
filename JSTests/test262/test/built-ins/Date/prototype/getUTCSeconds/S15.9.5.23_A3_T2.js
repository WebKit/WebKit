// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getUTCSeconds.length,
  true,
  'The value of `delete Date.prototype.getUTCSeconds.length` is expected to be true'
);

assert(
  !Date.prototype.getUTCSeconds.hasOwnProperty('length'),
  'The value of !Date.prototype.getUTCSeconds.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
