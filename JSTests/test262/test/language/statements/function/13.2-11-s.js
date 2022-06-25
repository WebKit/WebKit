// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 13.2-11-s
description: >
    StrictMode - enumerating over a function object looking for
    'caller' fails outside of the function
---*/

        var foo = Function("'use strict';");
        
        for (var tempIndex in foo) {
            assert.notSameValue(tempIndex, "caller", 'tempIndex');
        }
