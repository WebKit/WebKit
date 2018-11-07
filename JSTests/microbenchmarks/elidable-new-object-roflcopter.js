//@ skip if $architecture == "x86"

function sumOfArithSeries(limit) {
    return limit * (limit + 1) / 2;
}

var n = 5;

function foo() {
    var result = 0;
    for (var i = 0; i < n; ++i) {
        var o = {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: i}}}}}}}}}}}}}}}}}}};
        var p = {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: {f: i + 1}}}}}}}}}}}}}}}}}}};
        result += o.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f + p.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f.f;
    }
    return result;
}

for (var _i = 0; _i < 80000; ++_i) {
    var result = foo();
    if (result != sumOfArithSeries(n - 1) + sumOfArithSeries(n))
        throw "Error: bad result: " + result;
}
