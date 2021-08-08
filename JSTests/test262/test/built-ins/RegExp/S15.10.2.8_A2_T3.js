// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The form (?! Disjunction ) specifies a zero-width negative lookahead.
    In order for it to succeed, the pattern inside Disjunction must fail to match at the current position.
    The current position is not advanced before matching the sequel
es5id: 15.10.2.8_A2_T3
description: >
    Execute /Java(?!Script)([A-Z]\w*)/.test("using of Java language")
    and check results
---*/

var __executed = /Java(?!Script)([A-Z]\w*)/.test("using of Java language");

//CHECK#1
if (__executed) {
	throw new Test262Error('#1: /Java(?!Script)([A-Z]\\w*)/.test("using of Java language") === false');
}
