// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.setutcfullyear
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setUTCFullYear,
  false,
  'The value of delete Date.prototype.setUTCFullYear is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setUTCFullYear'),
  'The value of !Date.prototype.hasOwnProperty(\'setUTCFullYear\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
