// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date property "parse" has { DontEnum } attributes
esid: sec-date.parse
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.parse, false, 'The value of delete Date.parse is not false');
assert(!Date.hasOwnProperty('parse'), 'The value of !Date.hasOwnProperty(\'parse\') is expected to be true');

// TODO: Convert to verifyProperty() format.
