// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Refer 13.1; 
    It is a SyntaxError if any Identifier value occurs more than once within a FormalParameterList of a strict mode
    FunctionDeclaration or FunctionExpression.
es5id: 13.1-29-s
description: >
    Strict Mode - SyntaxError is thrown if a function is created using
    a FunctionExpression that is contained in eval strict code and the
    function has two identical parameters
flags: [noStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("'use strict'; var _13_1_29_fun = function (param, param) { };");
});
