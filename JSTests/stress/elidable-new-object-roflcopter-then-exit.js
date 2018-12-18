//@ skip if $architecture != "arm64" and $architecture != "x86-64"

function sumOfArithSeries(limit) {
    return limit * (limit + 1) / 2;
}

var n = 1000000;

var array = [42, "hello"];

function foo() {
    var result = 0;
    var q;
    for (var i = 0; i < n; ++i) {
        var o = {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: i}}}}}}}}}}}}}}}}}}};
        var p = {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: i + 1}}}}}}}}}}}}}}}}}}};
        q = array[(i > n - 100) | 0] + 1;
        result += o.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f + p.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f;
    }
    return q + result;
}

var result = foo();
if (result != "hello" + 1 + (sumOfArithSeries(n - 1) + sumOfArithSeries(n)))
    throw "Error: bad result: " + result;
