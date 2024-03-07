(function() {
    var proxy = new Proxy(function() {}, {});

    for (var i = 0; i < 1e6; ++i)
        proxy();
})();
