//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function testMax () {
    var one = SIMD.Float32x4(NaN, 42, NaN, NaN)
    var two = SIMD.Float32x4(NaN, NaN, NaN, 3.14)
    var three = SIMD.Float32x4.max(one,two);
    return three;
}

function testMin () {
    var one = SIMD.Float32x4(NaN, 42, NaN, NaN)
    var two = SIMD.Float32x4(NaN, NaN, NaN, 3.14)
    var three = SIMD.Float32x4.min(one,two);
    return three;
}


for (var i = 0; i < 3; i++) {
    testMax();
    testMin();
}

print("testMin = " + testMin());
print("testMax = " + testMax());

