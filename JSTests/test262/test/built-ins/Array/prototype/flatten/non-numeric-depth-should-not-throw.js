// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    if the argument is a string or object, the depthNum is 0
includes: [compareArray.js]
features: [Array.prototype.flatten]
---*/

var a = [1, [2]];
var expected = a;

// non integral string depthNum is converted to 0
var depthNum = 'TestString';
var actual = a.flatten(depthNum);
assert(compareArray(actual, expected), 'non integral string depthNum');

// object type depthNum is converted to 0
var depthNum = {};
var actual = a.flatten(depthNum);
assert(compareArray(actual, expected), 'object type depthNum');

// negative infinity depthNum is converted to 0
var depthNum = Number.NEGATIVE_INFINITY;
var actual = a.flatten(depthNum);
assert(compareArray(actual, expected), 'negative infinity depthNum');

// positive zero depthNum is converted to 0
var depthNum = +0;
var actual = a.flatten(depthNum);
assert(compareArray(actual, expected), 'positive zero depthNum');

// negative zero depthNum is converted to 0
var depthNum = -0;
var actual = a.flatten(depthNum);
assert(compareArray(actual, expected), 'negative zero depthNum');

// integral string depthNum is converted to an integer
var depthNum = '1';
var actual = a.flatten(depthNum);
var expected = [1, 2]
assert(compareArray(actual, expected), 'integral string depthNum');
