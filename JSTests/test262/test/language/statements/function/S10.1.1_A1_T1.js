// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Program functions are defined in source text by a FunctionDeclaration or created dynamically either
    by using a FunctionExpression or by using the built-in Function object as a constructor
es5id: 10.1.1_A1_T1
description: Defining function by a FunctionDeclaration
---*/

//CHECK#1
function f1(){
  return 1;
}
if(typeof(f1)!=="function")
  throw new Test262Error('#1: typeof(f1)!=="function"');
