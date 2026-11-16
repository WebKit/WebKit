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

// CSE: redundant Array.isArray calls on the same value should produce the same result.
(function testCSE() {
    function go(x) {
        let a = Array.isArray(x);
        let b = Array.isArray(x);
        let c = Array.isArray(x);
        return (a ? 1 : 0) + (b ? 1 : 0) + (c ? 1 : 0);
    }
    noInline(go);

    let arr = [1];
    let obj = {};
    let proxy = new Proxy([1], {});
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(go(arr), 3);
        shouldBe(go(obj), 0);
        shouldBe(go(42), 0);
        shouldBe(go(proxy), 3);
    }
})();

// ArrayIsArray must not clobber loads. obj.x should still be 1 after Array.isArray.
(function testNoClobber() {
    function go(x, obj) {
        let v = obj.x;
        let r = Array.isArray(x);
        return v + obj.x + (r ? 100 : 0);
    }
    noInline(go);

    let obj = { x: 1 };
    let arr = [1];
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(go(arr, obj), 102);
        shouldBe(go(obj, obj), 2);
    }
})();

// Revoked proxy must still throw, and must not be incorrectly CSE'd across the
// revoke() call (which is a normal Call node and clobbers MiscFields).
(function testRevokedProxy() {
    function go(x) {
        return Array.isArray(x);
    }
    noInline(go);

    for (let i = 0; i < testLoopCount; ++i) {
        let { proxy, revoke } = Proxy.revocable([1], {});
        shouldBe(go(proxy), true);
        revoke();
        shouldThrow(() => go(proxy), TypeError);
    }
})();

// Same-block CSE must observe revocation done by an effectful call between checks.
(function testNoCSEAcrossRevoke() {
    function go(x, revoke) {
        let a = Array.isArray(x);
        revoke();
        let b = Array.isArray(x);
        return a && b;
    }
    noInline(go);

    let nopRevoke = () => {};
    for (let i = 0; i < testLoopCount; ++i) {
        let { proxy, revoke } = Proxy.revocable([1], {});
        if (i & 1) {
            shouldThrow(() => go(proxy, revoke), TypeError);
        } else {
            shouldBe(go(proxy, nopRevoke), true);
            revoke();
        }
    }
})();

// Derived arrays.
(function testDerivedArray() {
    class Derived extends Array {}
    function go(x) {
        return Array.isArray(x) && Array.isArray(x);
    }
    noInline(go);

    let d = new Derived();
    let o = {};
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(go(d), true);
        shouldBe(go(o), false);
    }
})();
