function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, expectedError) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}!`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

shouldBe(delete Object.prototype.toString, true);

Array.prototype.join = {};
shouldBe(Array.prototype.toString.call({}), "[object Object]");

let revokeOnGet = false;
const { proxy, revoke } = Proxy.revocable([], {
    get: (target, key, receiver) => {
        if (revokeOnGet)
            revoke();
        return Reflect.get(target, key, receiver);
    },
});

shouldBe(Array.prototype.toString.call(proxy), "[object Array]");
revokeOnGet = true;
shouldThrow(() => { Array.prototype.toString.call(proxy); }, "TypeError: Array.isArray cannot be called on a Proxy that has been revoked");

Array.prototype.join = 1n;
shouldBe(Array.prototype.toString.call((function() { return arguments; })()), "[object Arguments]");
shouldBe(Array.prototype.toString.call(new Error), "[object Error]");

Array.prototype.join = Symbol();
shouldBe(Array.prototype.toString.call(new Boolean), "[object Boolean]");
shouldBe(Array.prototype.toString.call(new Number), "[object Number]");

Array.prototype.join = "join";
shouldBe(Array.prototype.toString.call(new String), "[object String]");
shouldBe(Array.prototype.toString.call(new Date), "[object Date]");

Array.prototype.join = 1;
shouldBe(Array.prototype.toString.call(new RegExp), "[object RegExp]");
shouldBe(Array.prototype.toString.call(new Proxy(() => {}, {})), "[object Function]");
shouldBe(Array.prototype.toString.call(new Proxy(new Date, {})), "[object Object]");

Array.prototype.join = true;
shouldBe(Array.prototype.toString.call({ [Symbol.toStringTag]: "Foo" }), "[object Foo]");
shouldBe(Array.prototype.toString.call(new Map), "[object Map]");

Array.prototype.join = null;
RegExp.prototype[Symbol.toStringTag] = "Foo";
shouldBe(Array.prototype.toString.call(new RegExp), "[object Foo]");
Number.prototype[Symbol.toStringTag] = Object("Foo"); // ignored
shouldBe(Array.prototype.toString.call(new Number), "[object Number]");

shouldBe(delete Array.prototype.join, true);

Object.defineProperty(JSON, Symbol.toStringTag, { value: "Foo" });
shouldBe(Array.prototype.toString.call(JSON), "[object Foo]");

shouldBe(delete Set.prototype[Symbol.toStringTag], true);
shouldBe(Array.prototype.toString.call(new Set), "[object Object]");

Object.defineProperty(Object.prototype, Symbol.toStringTag, { get: () => { throw new Error("@@toStringTag"); } });
shouldThrow(() => { Array.prototype.toString.call({}); }, "Error: @@toStringTag");
