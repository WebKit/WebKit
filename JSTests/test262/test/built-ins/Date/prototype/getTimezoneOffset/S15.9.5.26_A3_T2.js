// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getTimezoneOffset property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.gettimezoneoffset
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.getTimezoneOffset.length,
  true,
  'The value of `delete Date.prototype.getTimezoneOffset.length` is expected to be true'
);

assert(
  !Date.prototype.getTimezoneOffset.hasOwnProperty('length'),
  'The value of !Date.prototype.getTimezoneOffset.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
