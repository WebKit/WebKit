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
    
(function(f, o, p) {
    var result = 0;
    var n = 1000000;
    for (var i = 0; i < n; ++i) {
        result += fu(o);
        result += bar(f, o);
        var tmp = o;
        o = p;
        p = tmp;
    }
    if (result != n * o.f + n * o.e)
        throw "Error: bad result: " + result;
})(foo, {f:42, e:23}, {f:42, e:23, g:100});

