function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(fn, errorType) {
    let threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(`expected ${errorType.name} but got ${e}`);
    }
    if (!threw)
        throw new Error("did not throw");
}

// IsArray must not invoke any Proxy trap. This is the core assumption that makes
// the refined clobberize rule sound (no arbitrary JS execution).
(function testNoTrapInvocation() {
    let trapCalled = false;
    let handler = new Proxy({}, {
        get() { trapCalled = true; return undefined; }
    });
    let proxy = new Proxy([1], handler);
    function go(x) {
        return Array.isArray(x) && Array.isArray(x);
    }
    noInline(go);

    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(go(proxy), true);
    shouldBe(trapCalled, false);
})();

// Nested proxies, including a revoked proxy mid-chain.
(function testNestedProxy() {
    function go(x) {
        return Array.isArray(x);
    }
    noInline(go);

    let deep = [1];
    for (let i = 0; i < 10; ++i)
        deep = new Proxy(deep, {});
    let { proxy: inner, revoke } = Proxy.revocable([1], {});
    let outer = new Proxy(inner, {});

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(go(deep), true);
        shouldBe(go(outer), true);
    }
    revoke();
    shouldThrow(() => go(outer), TypeError);
})();

// write(SideState) must prevent LICM from hoisting a potentially-throwing
// ArrayIsArray past control flow that guards it.
(function testNoHoistPastGuard() {
    function go(x, cond, n) {
        let r = 0;
        for (let i = 0; i < n; ++i) {
            if (cond)
                r += Array.isArray(x) ? 1 : 0;
        }
        return r;
    }
    noInline(go);

    let arr = [1];
    let obj = {};
    for (let i = 0; i < testLoopCount; ++i) {
        go(arr, true, 10);
        go(obj, true, 10);
    }
    let { proxy, revoke } = Proxy.revocable([1], {});
    revoke();
    // cond=false: must not throw even though x is a revoked proxy.
    shouldBe(go(proxy, false, 100), 0);
    // cond=true: must throw on first iteration.
    shouldThrow(() => go(proxy, true, 100), TypeError);
})();
