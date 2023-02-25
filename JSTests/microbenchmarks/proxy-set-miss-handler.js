(function() {
    var proxy = new Proxy({}, {});

    for (var i = 0; i < 1e6; ++i)
        proxy.test = i;
})();
