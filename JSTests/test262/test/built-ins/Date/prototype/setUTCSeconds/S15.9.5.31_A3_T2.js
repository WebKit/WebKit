// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setUTCSeconds.length,
  true,
  'The value of `delete Date.prototype.setUTCSeconds.length` is expected to be true'
);

assert(
  !Date.prototype.setUTCSeconds.hasOwnProperty('length'),
  'The value of !Date.prototype.setUTCSeconds.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
