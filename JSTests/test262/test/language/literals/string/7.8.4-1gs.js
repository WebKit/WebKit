// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.8.4-1gs
description: >
    Strict Mode - OctalEscapeSequence(\0110) is forbidden in strict
    mode
negative:
  phase: parse
  type: SyntaxError
flags: [onlyStrict]
---*/

throw "Test262: This statement should not be evaluated.";

var _7_8_4_2 = '100abc\0110def';
