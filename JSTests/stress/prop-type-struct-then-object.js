function foo(o) {
    return o.f.g;
}

function bar(o, v) {
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {};
for (var i = 0; i < 5; ++i)
    bar(o, {g:42});

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result !== 42)
        throw "Error: bad result: " + result;
}

bar(o, Object.create({g:43}));
var result = foo(o);
if (result !== 43)
    throw "Error: bad result at end: " + result;
