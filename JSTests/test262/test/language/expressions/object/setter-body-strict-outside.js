// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es5id: 11.1.5_7-2-1-s
description: >
    Strict Mode - SyntaxError is thrown when an assignment to a
    reserved word is contained in strict code
negative:
  type: SyntaxError
  phase: parse
flags: [onlyStrict]
---*/

throw "Test262: This statement should not be evaluated.";

void {
  set x(value) {
    public = 42;
  }
};
