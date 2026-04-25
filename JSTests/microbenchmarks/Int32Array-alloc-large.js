//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ skip

for (var i = 0; i < 5000; ++i)
    new Int32Array(10000);
