description("Regression test for https://webkit.org/b/150580. - This test should not crash.");

// Verify that tail calls in the FTL properly handle 32 bit integer arguments when all registers are used.

function addEmUp(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, a32)
{
    "use strict";
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19 + a20 + a21 + a22 + a23 + a24 + a25 + a26 + a27 + a28 + a29 + a30 + a31 + a32;
}

noInline(addEmUp);

function sumVector(v)
{
    "use strict";
    return addEmUp(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15], v[16], v[17], v[18], v[19], v[20], v[21], v[22], v[23], v[24], v[25], v[26], v[27], v[28], v[29], v[30], v[31]);
}

noInline(sumVector);

function test()
{
    var buffer = new ArrayBuffer(128);
    var intValuesArray = new Int32Array(buffer);

    for (var i = 0; i < intValuesArray.length; i++)
        intValuesArray[i] = i + 1;

    for (var i = 0; i < 200000; i++) {
        if (sumVector(intValuesArray) != 528)
            testFailed("Didn't properly add elements of vector");
    }

    testPassed("Properly handled tail calls with 32 bit integer arguments when all registers are used.");
}

test();
