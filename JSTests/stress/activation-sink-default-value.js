//@ skip if $architecture == "x86"

var n = 10000000;

function bar(f) { f(10); }

function foo(b) {
    var result = 0;
    var imUndefined;
    var baz;
    var set = function (x) { result = x; return (imUndefined, baz); }
    baz = 40;
    if (b) {
        bar(set);
        if (result != 10)
            throw "Error: bad: " + result;
        if (baz !== 40)
            throw "Error: bad: " + baz;
        if (imUndefined !== void 0)
            throw "Error: bad value: " + imUndefined;
        return 0;
    }
    return result;
}

noInline(bar);
noInline(foo);

for (var i = 0; i < n; i++) {
    var result = foo(!(i % 100));
    if (result != 0)
        throw "Error: bad result: " + result;
}
