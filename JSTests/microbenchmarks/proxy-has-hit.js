(function() {
    var proxy = new Proxy({}, {
        has(target, propertyName) {
            return true;
        }
    });

    var has;
    for (var i = 0; i < 1e6; ++i)
        has = "test" in proxy;
})();
