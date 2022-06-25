// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toLocaleString" has { DontEnum } attributes
esid: sec-date.prototype.tolocalestring
description: Checking absence of DontDelete attribute
---*/

assert.notSameValue(
  delete Date.prototype.toLocaleString,
  false,
  'The value of delete Date.prototype.toLocaleString is not false'
);

assert(
  !Date.prototype.hasOwnProperty('toLocaleString'),
  'The value of !Date.prototype.hasOwnProperty(\'toLocaleString\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
