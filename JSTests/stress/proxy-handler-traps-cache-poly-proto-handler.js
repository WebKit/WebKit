function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() { this._field = 42; }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i)
        foo();
    return foo();
}

// A poly-proto object stores its prototype in the object rather than in its structure, so
// Object.setPrototypeOf leaves its StructureID untouched. The Proxy handler-traps cache must not
// treat a matching StructureID as proof that the prototype is still an object.

{
    let handler = makePolyProtoObject();
    Object.setPrototypeOf(handler, Object.prototype);
    handler.get = (target, key) => `got ${key}`;

    let proxy = new Proxy({ }, handler);
    shouldBe(proxy.foo, "got foo");

    Object.setPrototypeOf(handler, null);
    shouldBe(proxy.foo, "got foo");
    shouldBe(proxy.bar, "got bar");
}

{
    let handler = makePolyProtoObject();
    Object.setPrototypeOf(handler, Object.prototype);

    let proxy = new Proxy({ a: 1 }, handler);
    shouldBe(Object.keys(proxy).join(), "a");

    Object.setPrototypeOf(handler, null);
    shouldBe(Object.keys(proxy).join(), "a");

    handler.ownKeys = () => ["b"];
    handler.getOwnPropertyDescriptor = () => ({ value: 2, enumerable: true, configurable: true });
    shouldBe(Object.keys(proxy).join(), "b");
}

{
    let handler = makePolyProtoObject();
    Object.setPrototypeOf(handler, Object.prototype);
    handler.get = function (target, key) {
        Object.setPrototypeOf(handler, null);
        return 42;
    };

    let proxy = new Proxy({ }, handler);
    shouldBe(proxy.foo, 42);
    shouldBe(proxy.foo, 42);
}

// A poly-proto handler whose prototype gains a trap must pick the new trap up.
{
    let handler = makePolyProtoObject();
    Object.setPrototypeOf(handler, Object.prototype);

    let proxy = new Proxy({ foo: 1 }, handler);
    shouldBe(proxy.foo, 1);

    Object.setPrototypeOf(handler, { get: () => "from prototype" });
    shouldBe(proxy.foo, "from prototype");
}
