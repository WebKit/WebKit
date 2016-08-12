function foo(p, q, r) {
    while (r) {
        if (p)
            return 1;
        else if (p)
            return 2;
        else
            throw "error";
    }
}

function bar() {
    foo.apply(this, arguments);
}

function baz(a, b, c, d) {
    bar(a, b, c, d);
}

noInline(baz);

for (var i = 0; i < 10000; ++i)
    baz(1, 2, 3, 4);
