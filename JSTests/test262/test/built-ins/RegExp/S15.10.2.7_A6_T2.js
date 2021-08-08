// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: { DecimalDigits , }evaluates as follows:
    i) Let i be the MV of DecimalDigits
    ii) Return the two results i and \infty
es5id: 15.10.2.7_A6_T2
description: Execute /b{8,}c/.test("aaabbbbcccddeeeefffff") and check results
---*/

var __executed = /b{8,}c/.test("aaabbbbcccddeeeefffff");

//CHECK#1
if (__executed) {
	throw new Test262Error('#1: /b{8,}c/.test("aaabbbbcccddeeeefffff") === false');
}
