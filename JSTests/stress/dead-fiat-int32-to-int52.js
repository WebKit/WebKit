function foo(o) {
    fiatInt52(o.f);
    fiatInt52(o.f);
}

noInline(foo);

var o = {f:42};

for (var i = 0; i < 1000000; ++i)
    foo(o);
