//@ $skipModes << :lockdown if $buildType == "debug"

function sumOfArithSeries(limit) {
    return limit * (limit + 1) / 2;
}

var n = 10000000;

var array = [1, "hello"];

function foo() {
    var result = 0;
    var q;
    for (var i = 0; i < n; ++i) {
        var o = {f: i};
        var p = {f: i + 1};
        q = array[(i >= n - 100) | 0] + 1;
        result += o.f + p.f;
    }
    return q + result;
}

var result = foo();
if (result != "hello" + 1 + (sumOfArithSeries(n - 1) + sumOfArithSeries(n)))
    throw "Error: bad result: " + result;
