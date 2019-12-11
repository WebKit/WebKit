var o = {f:{f:{f:{f:{f:{f:{f:42}}}}}}};

function foo(p) {
    if (p)
        o.f.f.f.f.f.f = {f:53};
}

noInline(foo);

(function() {
    var n = 100000;
    var m = 100;
    var result = 0;
    
    for (var i = 0; i < n; ++i) {
        foo(i == n - m);
        result += o.f.f.f.f.f.f.f;
    }
    
    if (result != (n - m) * 42 + m * 53)
        throw "Error: bad result: " + result;
})();
