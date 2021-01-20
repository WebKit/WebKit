// Copyright (c) 2017 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Including sta.js will expose two functions:

        Test262Error
        $ERROR
---*/

assert(typeof Test262Error === "function");
assert(typeof Test262Error.prototype.toString === "function");
assert(typeof $ERROR === "function");
assert(typeof $DONOTEVALUATE === "function");
