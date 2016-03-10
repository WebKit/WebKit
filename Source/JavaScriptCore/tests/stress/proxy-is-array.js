function assert(b) {
    if (!b)
        throw new Error("Bad assertion.")
}

function test(f) {
    for (let i = 0; i < 500; i++)
        f();
}

test(function() {
    let proxy = new Proxy([], {});
    assert(Array.isArray(proxy));
});

test(function() {
    let {proxy, revoke} = Proxy.revocable([], {});
    assert(Array.isArray(proxy));

    revoke();
    let threw = false;
    try {
        Array.isArray(proxy);
    } catch(e) {
        threw = true;
        assert(e.toString() === "TypeError: Array.isArray can not be called on a Proxy that has been revoked.");
    }
    assert(threw);
});

test(function() {
    let proxyChain = new Proxy([], {});
    for (let i = 0; i < 400; i++)
        proxyChain = new Proxy(proxyChain, {});
    assert(Array.isArray(proxyChain));
});

test(function() {
    let proxyChain = new Proxy([], {});
    let revoke = null;
    for (let i = 0; i < 400; i++) {
        if (i !== 250) {
            proxyChain = new Proxy(proxyChain, {});
        } else {
            let result = Proxy.revocable(proxyChain, {});
            proxyChain = result.proxy;
            revoke = result.revoke;
        }
    }
    assert(Array.isArray(proxyChain));

    revoke();
    let threw = false;
    try {
        Array.isArray(proxyChain);
    } catch(e) {
        threw = true;
        assert(e.toString() === "TypeError: Array.isArray can not be called on a Proxy that has been revoked.");
    }
    assert(threw);
});
