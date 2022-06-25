// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleTimeString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking DontDelete attribute
---*/

assert.sameValue(
  delete Date.prototype.toLocaleTimeString.length,
  true,
  'The value of `delete Date.prototype.toLocaleTimeString.length` is expected to be true'
);

assert(
  !Date.prototype.toLocaleTimeString.hasOwnProperty('length'),
  'The value of !Date.prototype.toLocaleTimeString.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
