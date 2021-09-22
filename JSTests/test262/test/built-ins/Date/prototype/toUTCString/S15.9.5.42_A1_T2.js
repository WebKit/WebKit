// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toUTCString" has { DontEnum } attributes
esid: sec-date.prototype.toutcstring
description: Checking absence of DontDelete attribute
---*/

assert.notSameValue(
  delete Date.prototype.toUTCString,
  false,
  'The value of delete Date.prototype.toUTCString is not false'
);

assert(
  !Date.prototype.hasOwnProperty('toUTCString'),
  'The value of !Date.prototype.hasOwnProperty(\'toUTCString\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
