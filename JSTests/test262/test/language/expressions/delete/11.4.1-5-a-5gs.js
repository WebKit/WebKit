// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-5-a-5gs
description: >
    Strict Mode - SyntaxError is thrown when deleting a variable which
    is primitive type(boolean)
negative:
  phase: parse
  type: SyntaxError
flags: [onlyStrict]
---*/

throw "Test262: This statement should not be evaluated.";

var _11_4_1_5 = 7;
delete _11_4_1_5;
