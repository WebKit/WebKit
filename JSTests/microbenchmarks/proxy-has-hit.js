(function() {
    var proxy = new Proxy({}, {
        has(target, propertyName) {
            return true;
        }
    });

    var has;
    for (var i = 0; i < 1e7; ++i)
        has = "test" in proxy;
})();
