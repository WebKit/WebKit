//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

function asmModule(stdlib, imports, buffer) {
    "use asm";

    var log = stdlib.Math.log;
    var toF = stdlib.Math.fround;
    var imul = stdlib.Math.imul;

    var i4 = stdlib.SIMD.Int32x4;
    var i4store = i4.store;
    var i4swizzle = i4.swizzle;
    var i4check = i4.check;

    var f4 = stdlib.SIMD.Float32x4;
    var f4equal = f4.equal;
    var f4lessThan = f4.lessThan;
    var f4splat = f4.splat;
    var f4store = f4.store;
    var f4load = f4.load;
    var f4check = f4.check;
    var f4abs = f4.abs;
    var f4add = f4.add;
    var f4sub = f4.sub;

    var Float32Heap = new stdlib.Float32Array(buffer);
    var Int32Heap = new stdlib.Int32Array(buffer);
    var BLOCK_SIZE = 4;

    function matrixAddition(aIndex, bIndex, cIndex) {
        aIndex = aIndex | 0;
        bIndex = bIndex | 0;
        cIndex = cIndex | 0;

        var i = 0, dim1 = 0, dim2 = 0, matrixSize = 0;
        var aPiece = f4(0.0, 0.0, 0.0, 0.0), bPiece = f4(0.0, 0.0, 0.0, 0.0);

        dim1 = Int32Heap[aIndex << 2 >> 2] | 0;
        dim2 = Int32Heap[aIndex + 1 << 2 >> 2] | 0;
        matrixSize = imul(dim1, dim2);

        //array dimensions don't match
        if (((dim2 | 0) != (Int32Heap[bIndex + 1 << 2 >> 2] | 0)) | ((dim1 | 0) != (Int32Heap[bIndex << 2 >> 2] | 0))) {
            return -1;
        }

        Int32Heap[cIndex << 2 >> 2] = dim1;
        Int32Heap[cIndex + 1 << 2 >> 2] = dim2;

        while ((i|0) < (matrixSize|0)) {
            aPiece = f4load(Float32Heap, aIndex + 2 + i << 2 >> 2);
            bPiece = f4load(Float32Heap, bIndex + 2 + i << 2 >> 2);
            f4store(Float32Heap, cIndex + 2 + i << 2 >> 2, f4add(aPiece, bPiece));

            i = (i + BLOCK_SIZE)|0;
        }

        return 0;
    }

    function new2DMatrix(startIndex, dim1, dim2) {
        startIndex = startIndex | 0;
        dim1 = dim1 | 0;
        dim2 = dim2 | 0;

        var i = 0, matrixSize = 0;
        matrixSize = imul(dim1, dim2);
        Int32Heap[startIndex << 2 >> 2] = dim1;
        Int32Heap[startIndex + 1 << 2 >> 2] = dim2;
        for (i = 0; (i|0) < ((matrixSize - BLOCK_SIZE)|0); i = (i + BLOCK_SIZE)|0) {
            f4store(Float32Heap, startIndex + 2 + i << 2 >> 2, f4(toF((i + 1)|0), toF((i + 2)|0), toF((i + 3)|0), toF((i + 4)|0)));
        }
        for (; (i|0) < (matrixSize|0); i = (i + 1)|0) {
            Float32Heap[(startIndex + 2 + i) << 2 >> 2] = toF((i + 1)|0);
        }
        return (startIndex + 2 + i) | 0;
    }

    return {
        new2DMatrix: new2DMatrix,
        matrixAddition: matrixAddition
    };
}

function verify_results(type, results_ex, buffer, count)
{
    var i4;
        for (var i = 0, idx = 0; i < count/* * 16*/; i += 4)
    {
        i4 = type.load(buffer, i);
                equalSimd(results_ex[idx++], i4, type, "Matrix Add" );
        }
}

var exp_results = [
SIMD.Float32x4(2,4,6,8),
SIMD.Float32x4(10,12,14,16),
SIMD.Float32x4(18,20,22,24),
SIMD.Float32x4(26,28,30,32),
SIMD.Float32x4(34,36,38,40),
SIMD.Float32x4(42,44,46,48),
SIMD.Float32x4(50,52,54,56),
SIMD.Float32x4(58,60,62,64),
SIMD.Float32x4(66,68,70,72),
SIMD.Float32x4(74,76,78,80),
SIMD.Float32x4(82,84,86,88),
SIMD.Float32x4(90,92,94,96),
SIMD.Float32x4(98,100,102,104),
SIMD.Float32x4(106,108,110,112),
SIMD.Float32x4(114,116,118,120),
SIMD.Float32x4(122,124,126,128),
SIMD.Float32x4(130,132,134,136),
SIMD.Float32x4(138,140,142,144),
SIMD.Float32x4(146,148,150,152),
SIMD.Float32x4(154,156,158,160),
SIMD.Float32x4(162,164,166,168),
SIMD.Float32x4(170,172,174,176),
SIMD.Float32x4(178,180,182,184),
SIMD.Float32x4(186,188,190,192),
SIMD.Float32x4(194,196,198,200),
SIMD.Float32x4(202,204,206,208),
SIMD.Float32x4(210,212,214,216),
SIMD.Float32x4(218,220,222,224),
SIMD.Float32x4(226,228,230,232),
SIMD.Float32x4(234,236,238,240),
SIMD.Float32x4(242,244,246,248),
SIMD.Float32x4(250,252,254,256),
SIMD.Float32x4(258,260,262,264),
SIMD.Float32x4(266,268,270,272),
SIMD.Float32x4(274,276,278,280),
SIMD.Float32x4(282,284,286,288),
SIMD.Float32x4(290,292,294,296),
SIMD.Float32x4(298,300,302,304),
SIMD.Float32x4(306,308,310,312),
SIMD.Float32x4(314,316,318,320),
SIMD.Float32x4(322,324,326,328),
SIMD.Float32x4(330,332,334,336),
SIMD.Float32x4(338,340,342,344),
SIMD.Float32x4(346,348,350,352),
SIMD.Float32x4(354,356,358,360),
SIMD.Float32x4(362,364,366,368),
SIMD.Float32x4(370,372,374,376),
SIMD.Float32x4(378,380,382,384),
SIMD.Float32x4(386,388,390,392),
SIMD.Float32x4(394,396,398,400),
SIMD.Float32x4(402,404,406,408),
SIMD.Float32x4(410,412,414,416),
SIMD.Float32x4(418,420,422,424),
SIMD.Float32x4(426,428,430,432)
];


var buffer = new ArrayBuffer(16 * 1024 * 1024);
var m = asmModule(this, null, buffer);

m.new2DMatrix(0, 18, 12);
m.new2DMatrix(500, 18, 12);
m.matrixAddition(0, 500, 1000);

var values = new Float32Array(buffer).subarray(1000 + 2);

verify_results(SIMD.Float32x4, exp_results, values, 54*4);
print("PASS");