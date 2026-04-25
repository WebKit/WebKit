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

    function matrixMultiplication(aIndex, bIndex, cIndex) {
        aIndex = aIndex | 0;
        bIndex = bIndex | 0;
        cIndex = cIndex | 0;

        var i = 0, j = 0, dim1 = 0, dim2 = 0, intersectionNum = 0, matrixSize = 0;

        var newPiece = f4(0.0, 0.0, 0.0, 0.0), cPiece = f4(0.0, 0.0, 0.0, 0.0);

        //array dimensions don't match
        if ((Int32Heap[aIndex + 1 << 2 >> 2] | 0) != (Int32Heap[bIndex << 2 >> 2] | 0)) {
            return -1;
        }

        dim1 = Int32Heap[aIndex << 2 >> 2] | 0;
        dim2 = Int32Heap[bIndex + 1 << 2 >> 2] | 0;
        intersectionNum = Int32Heap[bIndex << 2 >> 2] | 0;
        matrixSize = imul(dim1, dim2);

        Int32Heap[cIndex << 2 >> 2] = dim1;
        Int32Heap[cIndex + 1 << 2 >> 2] = dim2;

        while ((i|0) < (matrixSize|0)) {
            cPiece = f4(0.0, 0.0, 0.0, 0.0);
            j = 0;
            while ((j|0) < (intersectionNum|0)) {
                newPiece = f4(toF(getIntersectionPiece(aIndex, bIndex, dim2, i, 0, j)),
                            toF(getIntersectionPiece(aIndex, bIndex, dim2, i, 1, j)),
                            toF(getIntersectionPiece(aIndex, bIndex, dim2, i, 2, j)),
                            toF(getIntersectionPiece(aIndex, bIndex, dim2, i, 3, j)));
                cPiece = f4add(cPiece, newPiece);
                j = (j + 1)|0;
            }
            f4store(Float32Heap, cIndex + 2 + i << 2 >> 2, cPiece);

            i = (i + BLOCK_SIZE)|0;
        }

        return 0;
    }

    function getIntersectionPiece(aIndex, bIndex, dim2, resultBlock, resultIndex, intersectionNum) {
        aIndex = aIndex | 0;
        bIndex = bIndex | 0;
        dim2 = dim2 | 0;
        resultBlock = resultBlock | 0;
        resultIndex = resultIndex | 0;
        intersectionNum = intersectionNum | 0;
        var aElem = 0.0, bElem = 0.0, cElem = 0.0;

        aElem = +toF(getElement(aIndex, ((resultBlock | 0) / (dim2 | 0))|0, intersectionNum));
        bElem = +toF(getElement(bIndex, intersectionNum, (resultBlock + resultIndex)|0));

        //return toF(getElement(aIndex, ((resultBlock|0) / (dim2|0)), intersectionNum));
        return toF(aElem * bElem);
    }

    function getElement(start, row, column) {
        start = start | 0;
        row = row | 0;
        column = column | 0;
        var dim1 = 0, dim2 = 0;

        dim2 = Int32Heap[start << 2 >> 2] | 0;
        dim1 = Int32Heap[start + 1 << 2 >> 2] | 0;
        //return toF(Float32Heap[(602 + imul(row, dim1) + column) << 2 >> 2]);
        return toF(Float32Heap[(start + 2 + imul(row, dim1) + column) << 2 >> 2]);

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
        matrixMultiplication: matrixMultiplication
    };
}

function verify_results(type, results_ex, buffer, count)
{
    var i4;
        for (var i = 0, idx = 0; i < count/* * 16*/; i += 4)
    {
        i4 = type.load(buffer, i);
                equalSimd(results_ex[idx++], i4, type, "Matrix Mul" );
        }
}

var buffer = new ArrayBuffer(16 * 1024 * 1024);
var m = asmModule(this, null, buffer);

m.new2DMatrix(0, 4, 8);
m.new2DMatrix(200, 8, 12);
m.new2DMatrix(400, 4, 4);
m.new2DMatrix(600, 4, 4);
m.matrixMultiplication(0, 200, 800);

m.matrixMultiplication(400, 600, 1000);

exp_results1 = [
SIMD.Float32x4(2052,2088,2124,2160),
SIMD.Float32x4(2196,2232,2268,2304),
SIMD.Float32x4(2340,2376,2412,2448),
SIMD.Float32x4(4452,4536,4620,4704),
SIMD.Float32x4(4788,4872,4956,5040),
SIMD.Float32x4(5124,5208,5292,5376),
SIMD.Float32x4(6645,6762,6879,6996),
SIMD.Float32x4(7113,7230,7347,7464),
SIMD.Float32x4(7581,7698,7815,7932),
SIMD.Float32x4(8355,8490,8625,8760),
SIMD.Float32x4(8895,9030,9165,9300),
SIMD.Float32x4(9435,9570,9705,9840),
];

exp_results2 = [
SIMD.Float32x4(90,100,110,120),
SIMD.Float32x4(170,188,206,224),
SIMD.Float32x4(211,230,249,268),
SIMD.Float32x4(169,182,195,208),
];

var values = new Float32Array(buffer);
verify_results(SIMD.Float32x4, exp_results1, values.subarray(800 + 2), 12 * 4);

verify_results(SIMD.Float32x4, exp_results2, values.subarray(1000 + 2), 4 * 4);
print("PASS");