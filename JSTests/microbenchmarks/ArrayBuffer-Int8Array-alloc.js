//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ skip

for (var i = 0; i < 70000; ++i)
    new Int8Array(new ArrayBuffer(10));

