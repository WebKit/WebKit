function shouldThrowTypeErrorOfThisRealm(f) {
    try {
        f();
    } catch (error) {
        if (!(error instanceof TypeError))
            throw new Error(`bad error realm or type: ${String(error)}`);
        return;
    }
    throw new Error("no error thrown");
}

const OtherProxy = createGlobalObject().Proxy;

{
    const revocable = OtherProxy.revocable(function () {}, {});
    revocable.revoke();
    shouldThrowTypeErrorOfThisRealm(() => revocable.proxy());
    shouldThrowTypeErrorOfThisRealm(() => new revocable.proxy());
    shouldThrowTypeErrorOfThisRealm(() => Reflect.apply(revocable.proxy, undefined, []));
    shouldThrowTypeErrorOfThisRealm(() => Reflect.construct(revocable.proxy, []));
}

{
    const proxy = new OtherProxy(function () {}, { apply: {} });
    shouldThrowTypeErrorOfThisRealm(() => proxy());
}

{
    const proxy = new OtherProxy(function () {}, { apply: 42 });
    shouldThrowTypeErrorOfThisRealm(() => proxy());
}

{
    const proxy = new OtherProxy(function () {}, { construct: {} });
    shouldThrowTypeErrorOfThisRealm(() => new proxy());
}

{
    const proxy = new OtherProxy(function () {}, { construct: 42 });
    shouldThrowTypeErrorOfThisRealm(() => new proxy());
}

{
    const proxy = new OtherProxy(function () {}, { construct() { return 42; } });
    shouldThrowTypeErrorOfThisRealm(() => new proxy());
}

// A null or undefined trap means the operation is forwarded to the target rather than rejected.
{
    let called = 0;
    const proxy = new OtherProxy(function () { called++; }, { apply: null, construct: undefined });
    proxy();
    new proxy();
    if (called !== 2)
        throw new Error(`bad call count: ${called}`);
}
