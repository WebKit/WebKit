function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

{
    let log = [];
    let logger = new Proxy({}, { get(t, k) { log.push(k); } });
    Object.defineProperties({}, new Proxy({ a: { value: 0 }, b: { value: 1 } }, logger));
    shouldBe(log.join(), "ownKeys,getOwnPropertyDescriptor,get,getOwnPropertyDescriptor,get");
}

{
    let log = [];
    let logger = new Proxy({}, { get(t, k) { log.push(k); } });
    Object.create(null, new Proxy({ a: { value: 0 }, b: { value: 1 } }, logger));
    shouldBe(log.join(), "ownKeys,getOwnPropertyDescriptor,get,getOwnPropertyDescriptor,get");
}

{
    let log = [];
    let target = {};
    Object.defineProperty(target, "a", { value: { value: 0 }, enumerable: true });
    Object.defineProperty(target, "b", { value: { value: 1 }, enumerable: false });
    Object.defineProperty(target, "c", { value: { value: 2 }, enumerable: true });
    let props = new Proxy(target, new Proxy({}, { get(t, k) { log.push(k); } }));
    let result = Object.defineProperties({}, props);
    shouldBe(log.join(), "ownKeys,getOwnPropertyDescriptor,get,getOwnPropertyDescriptor,getOwnPropertyDescriptor,get");
    shouldBe(result.a, 0);
    shouldBe(result.hasOwnProperty("b"), false);
    shouldBe(result.c, 2);
}

{
    let log = [];
    let s = Symbol("s");
    let logger = new Proxy({}, { get(t, k) { log.push(k); } });
    let result = Object.defineProperties({}, new Proxy({ a: { value: 0 }, [s]: { value: 1 } }, logger));
    shouldBe(log.join(), "ownKeys,getOwnPropertyDescriptor,get,getOwnPropertyDescriptor,get");
    shouldBe(result.a, 0);
    shouldBe(result[s], 1);
}

{
    let result = {};
    let threw = false;
    try {
        Object.defineProperties(result, new Proxy({ a: { value: 0 }, b: 1 }, {}));
    } catch {
        threw = true;
    }
    shouldBe(threw, true);
    shouldBe(result.hasOwnProperty("a"), false);
}
