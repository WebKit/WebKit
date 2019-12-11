//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

function testreciprocalApproximation() {
    var a = SIMD.Float32x4(8.0, 4.0, 2.0, -2.0);
    var c = SIMD.Float32x4.reciprocalApproximation(a);

    equal(0.125, SIMD.Float32x4.extractLane(c, 0));
    equal(0.250, SIMD.Float32x4.extractLane(c, 1));
    equal(0.5, SIMD.Float32x4.extractLane(c, 2));
    equal(-0.5, SIMD.Float32x4.extractLane(c, 3));
}

function testreciprocalSqrtApproximation() {
    var a = SIMD.Float32x4(1.0, 0.25, 0.111111, 0.0625);
    var c = SIMD.Float32x4.reciprocalSqrtApproximation(a);
    equal(1.0, SIMD.Float32x4.extractLane(c, 0));
    equal(2.0, SIMD.Float32x4.extractLane(c, 1));
    equal(3.0, SIMD.Float32x4.extractLane(c, 2));
    equal(4.0, SIMD.Float32x4.extractLane(c, 3));
}


testreciprocalApproximation();
testreciprocalApproximation();
testreciprocalApproximation();
testreciprocalApproximation();
testreciprocalApproximation();
testreciprocalApproximation();
testreciprocalApproximation();

testreciprocalSqrtApproximation();
testreciprocalSqrtApproximation();
testreciprocalSqrtApproximation();
testreciprocalSqrtApproximation();
testreciprocalSqrtApproximation();
testreciprocalSqrtApproximation();
testreciprocalSqrtApproximation();
testreciprocalSqrtApproximation();
print("PASS");