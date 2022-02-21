// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: The Date property "UTC" has { DontEnum } attributes
es5id: 15.9.4.3_A1_T2
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.UTC, false, 'The value of delete Date.UTC is not false');
assert(!Date.hasOwnProperty('UTC'), 'The value of !Date.hasOwnProperty(\'UTC\') is expected to be true');

// TODO: Convert to verifyProperty() format.
