// This is like prop-type-struct-then-object.js, but it checks that the optimizing JITs emit the right type
// check above a hot put_by_id that starts polymorphic but is folded by AI.

function foo(o) {
    return o.f.g;
}

function bar(o, p, v) {
    if (isFinalTier() || o == p) {
        var tmp = p.f;
        o = p;
    }
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {f:{g:42}};
for (var i = 0; i < 10000; ++i) {
    bar(o, o, {g:42});
    bar({a:1, b:2}, o, {g:42});
}

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result !== 42)
        throw "Error: bad result: " + result;
}

bar(o, o, Object.create({g:43}));
var result = foo(o);
if (result !== 43)
    throw "Error: bad result at end: " + result;
