function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

{
    let ogTarget = {x: 20};
    let theTarget = new Proxy(ogTarget, {
        get: function(target, propName, proxyArg) {
            assert(target === ogTarget);
            if (propName === "y")
                return 45;
            return target[propName];
        }
    });
    let handler = {
        get: function(target, propName, proxyArg) {
            if (propName === "z")
                return 60;
            return target[propName];
        }
    };
    let proxy = new Proxy(theTarget, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy.x === 20);
        assert(proxy["x"] === 20);

        assert(proxy.y === 45);
        assert(proxy["y"] === 45);

        assert(proxy.z === 60);
        assert(proxy["z"] === 60);
    }
}
