//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
for (var i = 0; i < 1e4; ++i) {
    var sum = 0;
    [1, 2, 3, 4].forEach(function (value) {
        sum += value;
    });
}
