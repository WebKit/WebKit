function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

let theTarget = {
    x: 30
};

let handler = {
    get: function(target, propName, proxyArg) {
        assert(target === theTarget);
        assert(proxyArg === obj);
        if (propName === "y")
            return 45;
        assert(propName === "x");
        return target[propName];
    }
};

let proxy = new Proxy(theTarget, handler);

let obj = Object.create(proxy);

for (let i = 0; i < 1000; i++) {
    assert(obj.x === 30);
    assert(obj.y === 45);
}
