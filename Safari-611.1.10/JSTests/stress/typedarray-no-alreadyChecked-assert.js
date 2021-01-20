// This test should not cause an ASSERT in Debug builds.

function checkIntArray(arr) {
    let x = arr;
    arr instanceof Uint32Array;
    arr[65537];
    x[0];
}

function checkFloatArray(arr) {
    let x = arr;
    arr instanceof Float64Array;
    arr[65537];
    x[0];
}


var intArray = new Uint32Array(1024);
for (let i = 0; i < 10000; i++)
    checkIntArray(intArray);

var floatArray = new Float64Array(1024);
for (let i = 0; i < 10000; i++)
    checkFloatArray(floatArray);


