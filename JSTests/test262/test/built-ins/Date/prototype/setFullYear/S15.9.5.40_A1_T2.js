// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setFullYear" has { DontEnum } attributes
esid: sec-date.prototype.setfullyear
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setFullYear,
  false,
  'The value of delete Date.prototype.setFullYear is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setFullYear'),
  'The value of !Date.prototype.hasOwnProperty(\'setFullYear\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
