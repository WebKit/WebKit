// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6-20
description: >
    7.6 - SyntaxError expected: reserved words used as Identifier
    Names in UTF8: while (while)
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

var \u0077\u0068\u0069\u006c\u0065 = 123;
