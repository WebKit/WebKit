// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "HexIntegerLiteral :: 0(x/X) is incorrect"
es5id: 7.8.3_A6.1_T2
description: Checking if execution of "0X" passes
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

//CHECK#1
0X
