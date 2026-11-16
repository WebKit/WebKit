//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ skip if $architecture == "x86"
//@ $skipModes << :lockdown if $buildType == "debug"

function sumOfArithSeries(limit) {
    return limit * (limit + 1) / 2;
}

var n = 10000000;

function foo() {
    var result = 0;
    for (var i = 0; i < n; ++i) {
        var o = {f: {f:i}};
        var p = {f: {f:i + 1}};
        result += o.f.f + p.f.f;
    }
    return result;
}

var result = foo();
if (result != sumOfArithSeries(n - 1) + sumOfArithSeries(n))
    throw "Error: bad result: " + result;
