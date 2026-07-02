(function() {
    var desc = { value: 1 };
    var proxy = new Proxy({}, {
        defineProperty(target, propertyName, desc) {
            return true;
        }
    });

    for (var i = 0; i < 1e6; ++i)
        Object.defineProperty(proxy, "test", desc);
})();
