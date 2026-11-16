//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

function testInt32x4BitConversion() {
    
    var m = SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000);
    var n = SIMD.Float32x4.fromInt32x4Bits(m);
    
    equalSimd(SIMD.Float32x4(1, 2, 3, 4), n, SIMD.Float32x4, "Test1");
    
    m = SIMD.Float32x4(5.0, 6.0, 7.0, 8.0);
    n = SIMD.Int32x4.fromFloat32x4Bits(m);
    equalSimd(SIMD.Int32x4(0x40A00000, 0x40C00000, 0x40E00000, 0x41000000), n, SIMD.Int32x4, "Test2");

    // Flip sign using bit-wise operators.
    n = SIMD.Float32x4(9.0, 10.0, 11.0, 12.0);
    m = SIMD.Int32x4(0x80000000, 0x80000000, 0x80000000, 0x80000000);
    var nMask = SIMD.Int32x4.fromFloat32x4Bits(n);
    nMask = SIMD.Int32x4.xor(nMask, m); // flip sign.
    n = SIMD.Float32x4.fromInt32x4Bits(nMask);
    
    equalSimd(SIMD.Float32x4(-9, -10, -11, -12), n, SIMD.Float32x4, "Test3");
    
    nMask = SIMD.Int32x4.fromFloat32x4Bits(n);
    nMask = SIMD.Int32x4.xor(nMask, m); // flip sign.
    n = SIMD.Float32x4.fromInt32x4Bits(nMask);
    equal(9.0, SIMD.Float32x4.extractLane(n, 0));
    equal(10.0, SIMD.Float32x4.extractLane(n, 1));
    equal(11.0, SIMD.Float32x4.extractLane(n, 2));
    equal(12.0, SIMD.Float32x4.extractLane(n, 3));
    equalSimd(SIMD.Float32x4(9, 10, 11, 12), n, SIMD.Float32x4, "Test4");
    
    // Should stay unmodified across bit conversions
    m = SIMD.Int32x4(0xFFFFFFFF, 0xFFFF0000, 0x80000000, 0x0);
    var m2 = SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.fromInt32x4Bits(m));
    equal(SIMD.Int32x4.extractLane(m, 0), SIMD.Int32x4.extractLane(m2, 0));
    equal(SIMD.Int32x4.extractLane(m, 1), SIMD.Int32x4.extractLane(m2, 1));
    equal(SIMD.Int32x4.extractLane(m, 2), SIMD.Int32x4.extractLane(m2, 2));
    equal(SIMD.Int32x4.extractLane(m, 3), SIMD.Int32x4.extractLane(m2, 3));
    equalSimd(m, m2, SIMD.Int32x4, "Test5");
}

function testFloat32x4Conversion() {
    var m = SIMD.Int32x4(1, 2, 3, 4);
    var n = SIMD.Float32x4.fromInt32x4(m);
    equalSimd(SIMD.Float32x4(1, 2, 3, 4), n, SIMD.Float32x4, "Test6");

    var a = SIMD.Float32x4(1.0, 2.0, 3.0, 4.0);
    var b = SIMD.Int32x4.fromFloat32x4(a);
    
    equalSimd(SIMD.Int32x4(1, 2, 3, 4), b, SIMD.Int32x4, "Test7");
    
    // in range
    a = SIMD.Float32x4(0x80000000 - 0x80, -0x80000000 + 0x80, 0x80000000 - 0x80, -0.0);
    try {b = SIMD.Int32x4.fromFloat32x4(a);} catch(e){print("Error8");}

    a = SIMD.Float32x4(0x80000000 - 0x80, 0x80000000 - 0x80, 0x80000000 - 0x80, -0.0);
    try {b = SIMD.Int32x4.fromFloat32x4(a);} catch(e){print("Error9");}
    
    a = SIMD.Float32x4(-0x80000000 + 0x80, -0x80000000 + 0x80, -0x80000000 + 0x80, -0.0);
    try {b = SIMD.Int32x4.fromFloat32x4(a);} catch(e){print("Error10");}
    
    // throws
    a = SIMD.Float32x4(0x80000000, -0x80000000, 0x80000000, -0.0);
    try {b = SIMD.Int32x4.fromFloat32x4(a); print("Error11");} catch(e){}
    
    a = SIMD.Float32x4(0x80000000, 0x80000000, 0x80000000, -0.0);
    try {b = SIMD.Int32x4.fromFloat32x4(a); print("Error12");} catch(e){}
    
    a = SIMD.Float32x4(-0x80000000 - 0x100, -0x80000000, -0x80000000, -0.0);
    try {b = SIMD.Int32x4.fromFloat32x4(a); print("Error13");} catch(e){}

}

testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();
testInt32x4BitConversion();

testFloat32x4Conversion();
testFloat32x4Conversion();
testFloat32x4Conversion();
testFloat32x4Conversion();
testFloat32x4Conversion();
testFloat32x4Conversion();
testFloat32x4Conversion();
testFloat32x4Conversion();
print("PASS");