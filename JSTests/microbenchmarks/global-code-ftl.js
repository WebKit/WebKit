//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var result = 0;
var n = 15000000;
for (var i = 0; i < n; ++i)
    result += {f: 1}.f;
if (result != n)
    throw "Error: bad result: " + result;

