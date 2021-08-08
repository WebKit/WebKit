// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production Assertion :: $ evaluates by returning an internal
    AssertionTester closure that takes a State argument x and performs the ...
es5id: 15.10.2.6_A1_T1
description: Execute /s$/.test("pairs\nmakes\tdouble") and check results
---*/

var __executed = /s$/.test("pairs\nmakes\tdouble");

//CHECK#1
if (__executed) {
	throw new Test262Error('#1: /s$/.test("pairs\\nmakes\\tdouble") === false');
}
