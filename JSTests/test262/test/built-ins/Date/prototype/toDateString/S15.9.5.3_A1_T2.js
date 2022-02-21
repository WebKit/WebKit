// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toDateString" has { DontEnum } attributes
esid: sec-date.prototype.todatestring
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.toDateString,
  false,
  'The value of delete Date.prototype.toDateString is not false'
);

assert(
  !Date.prototype.hasOwnProperty('toDateString'),
  'The value of !Date.prototype.hasOwnProperty(\'toDateString\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
