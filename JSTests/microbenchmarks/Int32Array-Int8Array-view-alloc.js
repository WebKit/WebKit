//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
for (var i = 0; i < 30000; ++i) {
    var array1 = new Int32Array(10);
    var array2 = new Int8Array(array1.buffer);
}

