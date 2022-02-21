// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCSeconds" has { DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setUTCSeconds,
  false,
  'The value of delete Date.prototype.setUTCSeconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setUTCSeconds'),
  'The value of !Date.prototype.hasOwnProperty(\'setUTCSeconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
