function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}, got: ${error}`);
}

function checkDescriptor(object, key, value, writable, enumerable, configurable) {
    let descriptor = Object.getOwnPropertyDescriptor(object, key);
    shouldBe(descriptor.value, value);
    shouldBe(descriptor.writable, writable);
    shouldBe(descriptor.enumerable, enumerable);
    shouldBe(descriptor.configurable, configurable);
}

function testResult() {
    let result = Proxy.revocable({}, {});
    shouldBe(Object.getPrototypeOf(result), Object.prototype);
    shouldBe(JSON.stringify(Reflect.ownKeys(result)), `["proxy","revoke"]`);
    checkDescriptor(result, "proxy", result.proxy, true, true, true);
    checkDescriptor(result, "revoke", result.revoke, true, true, true);

    let { proxy, revoke } = result;
    for (let i = 0; i < 10; ++i)
        result["p" + i] = i;
    shouldBe(result.proxy, proxy);
    shouldBe(result.revoke, revoke);
    for (let i = 0; i < 10; ++i)
        shouldBe(result["p" + i], i);
    shouldBe(delete result.proxy, true);
    shouldBe("proxy" in result, false);
    shouldBe(result.revoke, revoke);
    result.revoke = 42;
    shouldBe(result.revoke, 42);
}

function testRevokeFunction() {
    let { revoke } = Proxy.revocable({}, {});
    shouldBe(typeof revoke, "function");
    shouldBe(Object.getPrototypeOf(revoke), Function.prototype);
    shouldBe(JSON.stringify(Reflect.ownKeys(revoke)), `["length","name"]`);
    checkDescriptor(revoke, "length", 0, false, false, true);
    checkDescriptor(revoke, "name", "", false, false, true);
    shouldBe(revoke.toString(), "function () {\n    [native code]\n}");
    shouldThrow(() => new revoke, TypeError);
    shouldThrow(() => Reflect.construct(Object, [], revoke), TypeError);

    revoke.extra = 1;
    shouldBe(delete revoke.name, true);
    shouldBe(JSON.stringify(Reflect.ownKeys(revoke)), `["length","extra"]`);
    shouldBe(revoke(), undefined);
}

function testRevoke() {
    let target = { value: 1 };
    let first = Proxy.revocable(target, {});
    let second = Proxy.revocable(target, {});
    shouldBe(first.revoke === second.revoke, false);
    shouldBe(first.proxy.value, 1);
    shouldBe(second.proxy.value, 1);

    shouldBe(first.revoke.call(second.proxy, second.proxy), undefined);
    shouldThrow(() => first.proxy.value, TypeError);
    shouldBe(second.proxy.value, 1);

    shouldBe(first.revoke(), undefined);
    shouldBe(second.proxy.value, 1);

    shouldBe(second.revoke(), undefined);
    shouldThrow(() => second.proxy.value, TypeError);
}

function testCallableTarget() {
    let { proxy, revoke } = Proxy.revocable(function () { return 42; }, {});
    shouldBe(typeof proxy, "function");
    shouldBe(proxy(), 42);
    revoke();
    shouldBe(typeof proxy, "function");
    shouldThrow(() => proxy(), TypeError);
    shouldThrow(() => new proxy, TypeError);
}

function testRealm() {
    let other = createGlobalObject();
    let result = other.Proxy.revocable({}, {});
    shouldBe(Object.getPrototypeOf(result), other.Object.prototype);
    shouldBe(Object.getPrototypeOf(result.revoke), other.Function.prototype);
    shouldBe(JSON.stringify(Reflect.ownKeys(result)), `["proxy","revoke"]`);
    result.revoke();
    shouldThrow(() => result.proxy.value, TypeError);
}

testResult();
testRevokeFunction();
testCallableTarget();
testRealm();
for (let i = 0; i < testLoopCount; ++i)
    testRevoke();
