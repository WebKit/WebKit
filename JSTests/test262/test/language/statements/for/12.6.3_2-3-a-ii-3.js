// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Refer 12.6.3; 
    The production 
    IterationStatement : for ( var VariableDeclarationListNoIn ; Expressionopt ; Expressionopt ) Statement
    is evaluated as follows:
es5id: 12.6.3_2-3-a-ii-3
description: >
    The for Statement - (normal, V, empty) will be returned when first
    Expression is a Number object (value is NaN)
---*/

        var accessed = false;
        var numObj = new Number(NaN);
        for (var i = 0; numObj;) {
            accessed = true;
            break;
        }

assert(accessed, 'accessed !== true');
