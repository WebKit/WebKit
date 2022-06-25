// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setMinutes,
  false,
  'The value of delete Date.prototype.setMinutes is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setMinutes'),
  'The value of !Date.prototype.hasOwnProperty(\'setMinutes\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
