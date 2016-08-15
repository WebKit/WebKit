function bar(o, p) {
    var o2 = {f: 0};
    if (p)
        o2.f = o;
    return +o2.f;
}

var globalResult;
Object.prototype.valueOf = function() { globalResult = 1; };

function foo(p, q) {
    globalResult = 0;
    var o = arguments;
    if (p)
        bar(o, q);
    return globalResult;
}

noInline(foo);

foo(true, false);

for (var i = 0; i < 10000; ++i) {
    bar(1, true);
    bar({}, false);
}

for (var i = 0; i < 10000; ++i) {
    var result = foo(false, true);
    if (result !== 0)
        throw "Error: bad result: " + result;
}

var result = foo(true, true);
if (result !== 1)
    throw "Error: bad result at end: " + result;

