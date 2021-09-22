// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toTimeString" has { DontEnum } attributes
esid: sec-date.prototype.totimestring
description: Checking absence of DontDelete attribute
---*/

assert.notSameValue(
  delete Date.prototype.toTimeString,
  false,
  'The value of delete Date.prototype.toTimeString is not false'
);

assert(
  !Date.prototype.hasOwnProperty('toTimeString'),
  'The value of !Date.prototype.hasOwnProperty(\'toTimeString\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
