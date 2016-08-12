function bar(o, p) {
    if (p)
        return +o.f;
    return 42;
}

function foo(p, q) {
    var result = 0;
    var o = {valueOf: function() { result = 1; }};
    var o2 = {f: o};
    if (p)
        bar(o2, q);
    return result;
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

