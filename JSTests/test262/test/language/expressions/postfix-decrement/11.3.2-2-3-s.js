// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.3.2-2-3-s
description: >
    SyntaxError is not thrown if the identifier 'arguments[...]' appears as a
    PostfixExpression(arguments--)
---*/

function testcase() {
        arguments[1] = 7;
        arguments[1]--;
        assert.sameValue(arguments[1], 6, 'arguments[1]');
    }
testcase();
