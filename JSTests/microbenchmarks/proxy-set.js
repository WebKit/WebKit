(function() {
    var proxy = new Proxy({}, {
        set(target, propertyName, value, receiver) {
            return true;
        }
    });

    for (var i = 0; i < 1e7; ++i)
        proxy.test = i;
})();
