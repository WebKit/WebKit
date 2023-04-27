var proxy = new Proxy({}, {
    get(target, propertyName, receiver) {
        return 42;
    }
});

for (var i = 0; i < 1e7; ++i)
    proxy.test;
