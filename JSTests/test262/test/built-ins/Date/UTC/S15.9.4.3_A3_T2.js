// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: |
    The Date.UTC property "length" has { ReadOnly, ! DontDelete, DontEnum }
    attributes
es5id: 15.9.4.3_A3_T2
description: Checking DontDelete attribute
---*/

assert.sameValue(delete Date.UTC.length, true, 'The value of `delete Date.UTC.length` is expected to be true');

assert(
  !Date.UTC.hasOwnProperty('length'),
  'The value of !Date.UTC.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
