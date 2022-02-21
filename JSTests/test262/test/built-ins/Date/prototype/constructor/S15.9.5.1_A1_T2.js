// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "constructor" has { DontEnum } attributes
esid: sec-date.prototype.constructor
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.constructor,
  false,
  'The value of delete Date.prototype.constructor is not false'
);

assert(
  !Date.prototype.hasOwnProperty('constructor'),
  'The value of !Date.prototype.hasOwnProperty(\'constructor\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
