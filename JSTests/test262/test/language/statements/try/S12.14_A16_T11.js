// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    TryStatement: "try Block Catch" or "try Block Finally" or "try Block
    Catch Finally"
es5id: 12.14_A16_T11
description: >
    Catch and Finally are placed into the Block of "try" (whitle
    expected outside)
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

// CHECK#1
try{
  {
  }
  catch(e){}
  finally{}
}
