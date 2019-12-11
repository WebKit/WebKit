//@ skip if $architecture == "x86"

var n = 10000000;

function bar(f) { f(10); }

function foo(b) {
    var result = 0;
    var set = function (x) { result = x; }
    if (b) {
        bar(set);
        if (result != 10)
            throw "Error: bad: " + result;
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
