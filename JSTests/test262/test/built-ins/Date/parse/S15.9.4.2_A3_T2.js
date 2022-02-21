// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.parse property "length" has { ReadOnly, ! DontDelete, DontEnum }
    attributes
esid: sec-date.parse
description: Checking DontDelete attribute
---*/
assert.sameValue(delete Date.parse.length, true, 'The value of `delete Date.parse.length` is expected to be true');

assert(
  !Date.parse.hasOwnProperty('length'),
  'The value of !Date.parse.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
