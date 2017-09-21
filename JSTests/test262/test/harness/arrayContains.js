// Copyright (C) 2017 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Assert that the contents of an array contains another array as a slice or sub-array
includes: [arrayContains.js]
---*/

const willMatch = [0, 1, 2];

assert(arrayContains([0, 1, 2, 3], willMatch));
assert(arrayContains([null, 0, 1, 2, 3], willMatch));
assert(arrayContains([undefined, 0, 1, 2, 3], willMatch));
assert(arrayContains([false, 0, 1, 2, 3], willMatch));
assert(arrayContains([NaN, 0, 1, 2, 3], willMatch));

const willNotMatch = [1, 0, 2];

assert(!arrayContains([1, /* intentional hole */, 2], willNotMatch), '[1, /* intentional hole */, 2], willNotMatch)');
assert(!arrayContains([1, null, 2], willNotMatch), '[1, null, 2], willNotMatch)');
assert(!arrayContains([1, undefined, 2], willNotMatch), '[1, undefined, 2], willNotMatch)');
assert(!arrayContains([1, false, 2], willNotMatch), '[1, false, 2], willNotMatch)');
assert(!arrayContains([1, NaN, 2], willNotMatch), '[1, NaN, 2], willNotMatch)');
