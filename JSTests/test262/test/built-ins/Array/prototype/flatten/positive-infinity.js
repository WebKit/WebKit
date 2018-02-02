// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    if the argument is a positive infinity, the depthNum is max depth of the array
includes: [compareArray.js]
features: [Array.prototype.flatten]
---*/

var a = [1, [2, [3, [4]]]]
assert(compareArray(a.flatten(Number.POSITIVE_INFINITY), [1, 2, 3, 4]), 'positive infinity depthNum');
