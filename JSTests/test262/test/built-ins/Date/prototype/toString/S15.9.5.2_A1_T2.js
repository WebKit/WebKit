// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toString" has { DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking absence of DontDelete attribute
---*/

assert.notSameValue(delete Date.prototype.toString, false, 'The value of delete Date.prototype.toString is not false');
assert(
  !Date.prototype.hasOwnProperty('toString'),
  'The value of !Date.prototype.hasOwnProperty(\'toString\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
