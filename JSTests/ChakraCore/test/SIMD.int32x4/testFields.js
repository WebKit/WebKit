//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function equal(a, b) {
    if (a == b)
        print("Correct")
    else
        print(">> Fail!")
}
var si = SIMD.Int32x4(10, -20, 30, 0);
function testScalarGetters() {
    print('Int32x4 scalar getters');
    var a = SIMD.Int32x4(1, 2, 3, 4);
    equal(1, SIMD.Int32x4.extractLane(a, 0));
    equal(2, SIMD.Int32x4.extractLane(a, 1));
    equal(3, SIMD.Int32x4.extractLane(a, 2));
    equal(4, SIMD.Int32x4.extractLane(a, 3));
}

function testExtractLane1() {
    print("I4 ExtractLane");

    print(typeof si);
    print(si.toString());

    print(typeof SIMD.Int32x4.extractLane(si, 0));
    print(SIMD.Int32x4.extractLane(si, 0).toString());

    print(typeof SIMD.Int32x4.extractLane(si, 1))
    print(SIMD.Int32x4.extractLane(si, 1).toString());

    print(typeof SIMD.Int32x4.extractLane(si, 2));
    print(SIMD.Int32x4.extractLane(si, 2).toString());

    print(typeof SIMD.Int32x4.extractLane(si, 3));
    print(SIMD.Int32x4.extractLane(si, 3).toString());
}

function testReplaceLane1() {
    print("I4 ReplaceLane");

    var v = SIMD.Int32x4.replaceLane(si, 0, 10)
    print(v.toString());

    v = SIMD.Int32x4.replaceLane(si, 1, 12)
    print(v.toString());

    v = SIMD.Int32x4.replaceLane(si, 2, -30)
    print(v.toString());

    v = SIMD.Int32x4.replaceLane(si, 3, 0)
    print(v.toString());
}

testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();
testScalarGetters();


testExtractLane1();
print();
testReplaceLane1();
print();

