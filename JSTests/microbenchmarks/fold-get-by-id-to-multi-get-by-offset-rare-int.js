function foo(o) {
    return o.f;
}

function fu(o) {
    return o.e;
}

function bar(f, o) {
    return f(o);
}

for (var i = 0; i < 1000; ++i) {
    var o = {f:1};
    o["i" + i] = 42;
    foo(o);
    fu({f:1, e:2});
    fu({e:1, f:2});
}
    
for (var i = 0; i < 100; ++i) {
    bar(foo, {f:1});
    bar(function() { }, null);
    bar(function() { return 42 }, null);
}

Number.prototype.f = 100;
    
(function(f, o, p) {
    var result = 0;
    var n = 1000000;
    for (var i = 0; i < n + 1; ++i) {
        result += fu(o);
        var q;
        if (i == n)
            q = 42;
        else
            q = o;
        result += bar(f, q);
        var tmp = o;
        o = p;
        p = tmp;
    }
    if (result != (o.f + o.e + p.f + p.e) * n / 2 + 100 + p.e)
        throw "Error: bad result: " + result;
})(foo, {f:42, e:43}, {e:44, f:45});

