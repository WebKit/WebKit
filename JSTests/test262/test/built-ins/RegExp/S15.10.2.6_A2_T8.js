// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production Assertion :: ^ evaluates by returning an internal
    AssertionTester closure that takes a State argument x and performs the ...
es5id: 15.10.2.6_A2_T8
description: Execute /^xxx/.test("yyyyy") and check results
---*/

var __executed = /^xxx/.test("yyyyy");

//CHECK#1
if (__executed) {
	throw new Test262Error('#1: /^xxx/.test("yyyyy") === false');
}
