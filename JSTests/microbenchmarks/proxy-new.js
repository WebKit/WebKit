(function() {
    var target = { x: 42 };
    var handler = {};
    var proxies = new Array(16).fill(null);

    for (var i = 0; i < 1e6; ++i)
        proxies[i & 15] = new Proxy(target, handler);

    for (var proxy of proxies) {
        if (proxy.x !== 42)
            throw new Error("bad value");
    }
})();
