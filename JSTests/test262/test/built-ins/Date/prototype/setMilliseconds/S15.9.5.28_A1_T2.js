// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMilliseconds" has { DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setMilliseconds,
  false,
  'The value of delete Date.prototype.setMilliseconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setMilliseconds'),
  'The value of !Date.prototype.hasOwnProperty(\'setMilliseconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
