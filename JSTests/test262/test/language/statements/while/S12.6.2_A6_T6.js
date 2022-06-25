// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Expression in "while" IterationStatement is bracketed with braces
es5id: 12.6.2_A6_T6
description: Checking if execution of "while 'hood' break" fails
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
while 'hood' break;
//
//////////////////////////////////////////////////////////////////////////////
