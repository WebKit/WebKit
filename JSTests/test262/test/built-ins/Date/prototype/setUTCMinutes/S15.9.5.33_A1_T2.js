// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setUTCMinutes,
  false,
  'The value of delete Date.prototype.setUTCMinutes is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setUTCMinutes'),
  'The value of !Date.prototype.hasOwnProperty(\'setUTCMinutes\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
