//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

var sf = SIMD.Float32x4(1.35, -2.0, 3.4, 0.0);
function testExtractLane() {
    
    equal(1.35,SIMD.Float32x4.extractLane(sf, 0));
    equal(-2.0, SIMD.Float32x4.extractLane(sf, 1));
    equal(3.4, SIMD.Float32x4.extractLane(sf, 2));
    equal(0.0, SIMD.Float32x4.extractLane(sf, 3));
}

function testReplaceLane() {
    var v = SIMD.Float32x4.replaceLane(sf, 0, 10.2)
    equalSimd([10.199999809265136, -2, 3.4000000953674316, 0], v, SIMD.Float32x4, "Replace Lane");
    v = SIMD.Float32x4.replaceLane(sf, 1, 12.3)
    equalSimd([1.350000023841858, 12.300000190734863, 3.4000000953674316, 0], v, SIMD.Float32x4, "Replace Lane");

    v = SIMD.Float32x4.replaceLane(sf, 2, -30.2)
    equalSimd([1.350000023841858, -2, -30.200000762939453, 0], v, SIMD.Float32x4, "Replace Lane");

    v = SIMD.Float32x4.replaceLane(sf, 3, 0.0)
    equalSimd([1.350000023841858, -2, 3.4000000953674316, 0], v, SIMD.Float32x4, "Replace Lane");

}

function testScalarGetters() {
    var a = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    equal(1.0, SIMD.Float32x4.extractLane(a, 0));
    equal(2.0, SIMD.Float32x4.extractLane(a, 1));
    equal(3.0, SIMD.Float32x4.extractLane(a, 2));
    equal(4.0, SIMD.Float32x4.extractLane(a, 3));
}



testScalarGetters();
testScalarGetters();
testScalarGetters();

testExtractLane();

testReplaceLane();
print("PASS");