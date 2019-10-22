//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var fe = function (a, b) { return a + b;}

noInline(fe);

function bar(a, b) {
    return fe(a, b);
}

noInline(bar);

for (var i = 0; i < 1000000; ++i) {
    var result = bar(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}
