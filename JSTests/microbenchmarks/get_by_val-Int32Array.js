//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var array = new Int32Array(42);
    for (var i = 0; i < 42; ++i)
        array[i] = i;
    var result = 0;
    for (var i = 0; i < 100000; ++i)
        result += array[i % array.length];
    if (result != 2049960)
        throw "Error: bad result: " + result;
})();
