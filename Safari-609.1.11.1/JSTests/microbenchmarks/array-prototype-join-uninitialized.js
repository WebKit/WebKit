//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runDefault
var N = 10 * 1024 * 1024
var s = Array(N).join();
if (s !== ",".repeat(N - 1))
    throw("Unexpected result of Array.prototype.join()");
