function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

{
    let { proxy, revoke } = Proxy.revocable([], {});
    revoke();

    let result = [proxy].flat(0);
    shouldBe(result.length, 1);
    shouldBe(result[0], proxy);
}

{
    let { proxy, revoke } = Proxy.revocable([1, 2, 3], {});
    revoke();

    let result = [proxy].flat(-1);
    shouldBe(result.length, 1);
    shouldBe(result[0], proxy);
}

{
    let lengthAccessed = false;
    let proxy = new Proxy({}, {
        get(t, k) {
            if (k === "length")
                lengthAccessed = true;
            return Reflect.get(t, k);
        }
    });
    let result = [proxy, 42].flat(0);
    shouldBe(result.length, 2);
    shouldBe(result[0], proxy);
    shouldBe(result[1], 42);
    shouldBe(lengthAccessed, false);
}

{
    let { proxy, revoke } = Proxy.revocable([], {});
    revoke();

    let didThrow = false;
    try {
        [proxy].flat(1);
    } catch (e) {
        didThrow = true;
        shouldBe(e instanceof TypeError, true);
    }
    shouldBe(didThrow, true);
}

{
    let { proxy, revoke } = Proxy.revocable([], {});
    revoke();

    let arrayLike = { length: 2, 0: proxy, 1: "x" };
    let result = Array.prototype.flat.call(arrayLike, 0);
    shouldBe(result.length, 2);
    shouldBe(result[0], proxy);
    shouldBe(result[1], "x");
}
