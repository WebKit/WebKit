// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: { DecimalDigits , DecimalDigits }
    evaluates as ...
es5id: 15.10.2.7_A1_T2
description: Execute /\d{2,4}/.test("the 7 movie") and check results
---*/

var __executed = /\d{2,4}/.test("the 7 movie");

//CHECK#1
if (__executed) {
	throw new Test262Error('#1: /\\d{2,4}/.test("the 7 movie") === false');
}
