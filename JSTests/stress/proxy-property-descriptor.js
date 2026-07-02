function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

let theTarget = {};
Object.defineProperty(theTarget, "x", {
    writable: false,
    configurable: false,
    value: 45
});

Object.defineProperty(theTarget, "y", {
    writable: false,
    configurable: false,
    value: 45
});

Object.defineProperty(theTarget, "getter", {
    configurable: false,
    set: function(x) { }
});

Object.defineProperty(theTarget, "badGetter", {
    configurable: false,
    set: function(x) { }
});

let handler = {
    get: function(target, propName, proxyArg) {
        assert(target === theTarget);
        assert(proxyArg === proxy);
        if (propName === "x")
            return 45;
        else if (propName === "y")
            return 30;
        else if (propName === "getter")
            return undefined;
        else {
            assert(propName === "badGetter");
            return 25;
        }
    }
};

let proxy = new Proxy(theTarget, handler);

for (let i = 0; i < 1000; i++) {
    assert(proxy.x === 45);
    assert(proxy["x"] === 45);
}

for (let i = 0; i < 1000; i++) {
    try {
        if (i % 2)
            proxy.y;
        else
            proxy["y"];
    } catch(e) {
        threw = true;
        assert(e.toString() === "TypeError: Proxy handler's 'get' result of a non-configurable and non-writable property should be the same value as the target's property");
    }
    assert(threw === true);
}

for (let i = 0; i < 1000; i++) {
    assert(proxy.getter === undefined);
    assert(proxy["getter"] === undefined);
}

for (let i = 0; i < 1000; i++) {
    try {
        if (i % 2)
            proxy.badGetter;
        else
            proxy["badGetter"];

    } catch(e) {
        threw = true;
        assert(e.toString() === "TypeError: Proxy handler's 'get' result of a non-configurable accessor property without a getter should be undefined");
    }
    assert(threw === true);
}
