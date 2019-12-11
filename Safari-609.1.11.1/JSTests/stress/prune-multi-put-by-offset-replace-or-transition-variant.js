function foo(o) {
    o.f = 1;
}

function fu(o) {
    o.e = 2;
}

function bar(f, o) {
    f(o);
}

function baz(f, o) {
    f(o);
}

for (var i = 0; i < 100; ++i) {
    foo({f:1, e:2});
    foo({e:1, f:2});
    foo({e:1});
    foo({d:1, e:2, f:3});
    fu({f:1, e:2});
    fu({e:1, f:2});
    fu({e:1, f:2, g:3});
    fu({d:1, e:2, f:3, g:4});
}
    
for (var i = 0; i < 100; ++i) {
    bar(foo, {f:1});
    bar(function() { }, null);
    bar(function() { return 42; }, null);
    baz(fu, {e:1});
    baz(function() { }, null);
    baz(function() { return 42; }, null);
}
    
(function(f, g, o, p) {
    var result = 0;
    var n = 1000000;
    for (var i = 0; i < n; ++i) {
        var q;
        if (i == n - 1)
            q = p;
        else
            q = o;
        baz(g, q);
        bar(f, q);
    }
    if (o.e != 2)
        throw "Error: bad value in o.e: " + o.e;
    if (o.f != 1)
        throw "Error: bad value in o.f: " + o.f;
    if (p.e != 2)
        throw "Error: bad value in p.e: " + p.e;
    if (p.f != 1)
        throw "Error: bad value in p.f: " + p.f;
})(foo, fu, {e:42, f:2}, {e:12, f:13, g:14});

