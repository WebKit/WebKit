(function() {
    var proxy = new Proxy({}, {
        getOwnPropertyDescriptor(target, propertyName) {}
    });

    var desc;
    for (var i = 0; i < 1e6; ++i)
        desc = Object.getOwnPropertyDescriptor(proxy, "test");
})();
