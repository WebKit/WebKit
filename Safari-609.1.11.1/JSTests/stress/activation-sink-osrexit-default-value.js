//@ skip if $architecture == "x86"

var n = 10000000;

function bar(set) { 
    var result = set(0);
    if (result !== void 0)
        throw "Error: bad value: " + result;
}

function foo(b) {
    var result = 0;
    var imUndefined;
    var baz;
    var set = function (x) { 
        result = x; 
        if (baz !== 50)
            throw "Error: bad value: " + baz;
        return imUndefined;
    }
    baz = 50;
    if (b) {
        OSRExit();
        if (b) {
            bar(set);
        }
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
