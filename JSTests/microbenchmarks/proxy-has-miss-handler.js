(function() {
    var proxy = new Proxy({}, {});

    var has;
    for (var i = 0; i < 1e7; ++i)
        has = "test" in proxy;
})();
