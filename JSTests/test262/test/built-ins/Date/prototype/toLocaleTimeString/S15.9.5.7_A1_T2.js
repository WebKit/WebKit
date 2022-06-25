// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleTimeString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking absence of DontDelete attribute
---*/

assert.notSameValue(
  delete Date.prototype.toLocaleTimeString,
  false,
  'The value of delete Date.prototype.toLocaleTimeString is not false'
);

assert(
  !Date.prototype.hasOwnProperty('toLocaleTimeString'),
  'The value of !Date.prototype.hasOwnProperty(\'toLocaleTimeString\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
