//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"

function sumOfArithSeries(limit) {
    return limit * (limit + 1) / 2;
}

var n = 10000000;

function bar() { }

function foo(b) {
    var result = 0;
    for (var i = 0; i < n; ++i) {
        var iter = ([i, i + 1])[Symbol.iterator]();
        if (b) {
            bar(o, p);
            return;
        }
        for (x of iter)
            result += x;
    }
    return result;
}

noInline(bar);
noInline(foo);

var result = foo(false);
if (result != sumOfArithSeries(n - 1) + sumOfArithSeries(n))
    throw "Error: bad result: " + result;
