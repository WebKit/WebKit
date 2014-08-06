var o = {f:{f:{f:{f:{f:{f:{f:42}}}}}}};
(function() {
    var n = 1000000;
    var result = 0;
    for (var i = 0; i < n; ++i)
        result += o.f.f.f.f.f.f.f;
    
    if (result != n * 42)
        throw "Error: bad result: " + result;
})();
