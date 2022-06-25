// This is like prop-type-struct-then-object.js, but it checks that the optimizing JITs emit the right type
// check above a hot put_by_id.

function foo(o) {
    return o.f.g;
}

function bar(o, v) {
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {f:{g:42}};
for (var i = 0; i < 10000; ++i)
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
