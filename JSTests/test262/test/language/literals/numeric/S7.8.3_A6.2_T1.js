// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: 0xG is incorrect
es5id: 7.8.3_A6.2_T1
description: Checking if execution of "0xG" fails
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

//CHECK#1
0xG
