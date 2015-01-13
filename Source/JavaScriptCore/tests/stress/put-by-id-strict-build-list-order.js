function foo(o) {
    "use strict";
    o.f = 42;
}

noInline(foo);

var a = {};
foo(a);
foo(a);
a = {f : 3};
foo(a);

var b = {};
foo(b);
foo(b);
