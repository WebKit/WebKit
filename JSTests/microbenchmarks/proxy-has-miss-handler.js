(function() {
    var proxy = new Proxy({}, {});

    var has;
    for (var i = 0; i < 1e6; ++i)
        has = "test" in proxy;
})();
