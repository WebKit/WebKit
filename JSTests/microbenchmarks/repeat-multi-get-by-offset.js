function foo(o) {
    return o.f + o.g;
}

noInline(foo);

var o = { f:1, g:2 };
var p = { g:3, f:4 };
for (var i = 0; i < 2000000; ++i) {
    foo(o);
    foo(p);
}
