function bar(o, p) {
    if (p)
        return +o.f;
    return 42;
}

var globalResult;
Function.prototype.valueOf = function() { globalResult = 1; };

function foo(p, q) {
    globalResult = 0;
    var o = function() { };
    var o2 = {f: o};
    if (p)
        bar(o2, q);
    return globalResult;
}

noInline(foo);

foo(true, false);

for (var i = 0; i < 10000; ++i)
    bar({f:42}, true);

for (var i = 0; i < 10000; ++i) {
    var result = foo(false, true);
    if (result !== 0)
        throw "Error: bad result: " + result;
}

var result = foo(true, true);
if (result !== 1)
    throw "Error: bad result at end: " + result;

