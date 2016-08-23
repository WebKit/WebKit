function foo(o) {
    return o.f;
}

function fu(o) {
    return o.e;
}

function bar(f, o) {
    return f(o);
}

for (var i = 0; i < 100; ++i) {
    foo({f:1, e:2});
    foo({e:1, f:2});
    foo({d:1, e:2, f:3});
    fu({f:1, e:2});
    fu({e:1, f:2});
    fu({d:1, e:2, f:3});
}
    
for (var i = 0; i < 100; ++i) {
    bar(foo, {f:1});
    bar(function() { }, null);
    bar(function() { return 42 }, null);
}
    
(function(f, o) {
    var result = 0;
    var n = 1000000;
    for (var i = 0; i < n; ++i) {
        var p;
        if (i == n - 1) // Defeat LICM.
            p = {f: 42, e: 42};
        else
            p = o;
        result += fu(p);
        result += bar(f, p);
    }
    if (result != (n - 1) * (o.f + o.e) + 42 * 2)
        throw "Error: bad result: " + result;
})(foo, {f:42, e:2});

