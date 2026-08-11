//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
for (var i = 0; i < 10; ++i) {
    for (var j = 0; j < 100000; ++j)
        new Array(10 * i);
}
