//@ skip if $architecture == "x86"

function sumOfArithSeries(limit) {
    return limit * (limit + 1) / 2;
}

var n = 10000000;

function foo() {
    var result = 0;
    for (var i = 0; i < n; ++i) {
        var leaf = {f:i};
        var o = {f:leaf};
        var p = {f:leaf};
        var q = {f:o, g:p};
        result += q.f.f.f;
    }
    return result;
}

var result = foo();
if (result != sumOfArithSeries(n - 1))
    throw "Error: bad result: " + result;
