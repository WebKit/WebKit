function foo(o) {
    return o.f;
}

function fu(o) {
    return o.e;
}

function bar(f, o) {
    return f(o);
}

function baz(f, o) {
    return f(o);
}

for (var i = 0; i < 100; ++i) {
    foo({f:1, e:2});
    foo({e:1, f:2});
    foo({d:1, e:2, f:3});
    fu({f:1, e:2});
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
        result += baz(g, q);
        result += bar(f, q);
    }
    if (result != (n - 1) * (o.f + o.e) + 12 + 13)
        throw "Error: bad result: " + result;
})(foo, fu, {f:42, e:2}, {e:12, f:13, g:14});

