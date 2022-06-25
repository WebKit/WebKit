// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Refer 13.1;
    It is a SyntaxError if any Identifier value occurs more than once within a FormalParameterList of a strict mode
    FunctionDeclaration or FunctionExpression.
es5id: 13.1-26-s
description: >
    Strict Mode - SyntaxError is thrown if a function is created using
    a FunctionDeclaration whose FunctionBody is contained in strict
    code and the function has two identical parameters which are
    separated by a unique parameter name
negative:
  phase: parse
  type: SyntaxError
flags: [noStrict]
---*/

$DONOTEVALUATE();

function _13_1_26_fun(param1, param2, param1) { 'use strict'; }
