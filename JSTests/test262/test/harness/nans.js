// Copyright (C) 2017 Rick Waldron, 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Including nans.js will expose:

        var distinctNaNs = [
          0/0, Infinity/Infinity, -(0/0), Math.pow(-1, 0.5), -Math.pow(-1, 0.5)
        ];

includes: [nans.js]
---*/

assert.sameValue(Number.isNaN(distinctNaNs[0]), true);
assert.sameValue(Number.isNaN(distinctNaNs[1]), true);
assert.sameValue(Number.isNaN(distinctNaNs[2]), true);
assert.sameValue(Number.isNaN(distinctNaNs[3]), true);
assert.sameValue(Number.isNaN(distinctNaNs[4]), true);
