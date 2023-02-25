function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(fn, expectedError) {
    let errorThrown = false;
    try {
        fn();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}



(function() {
    var setCalls = 0;
    var proxy = new Proxy({}, {
        set(target, propertyName, value, receiver) {
            "use strict";
            setCalls++;
            return value !== 1e7 - 10;
        }
    });

    for (var i = 0; i < 1e7; ++i)
        proxy.foo = i; // shouldn't throw

    shouldBe(setCalls, 1e7);
})();

shouldThrow(function() {
    "use strict";

    var proxy = new Proxy({}, {
        set(target, propertyName, value, receiver) {
            return value !== 1e7 - 10;
        }
    });

    for (var i = 0; i < 1e7; ++i)
        proxy.foo = i;
}, "TypeError: Proxy object's 'set' trap returned falsy value for property 'foo'");

(function() {
    var setCalls = 0;

    var target = {};
    var handler = { set() { "use strict"; setCalls++; return true; } };
    var proxy = new Proxy(target, handler);
    Object.defineProperty(target, "foo", { value: {}, writable: false, enumerable: true, configurable: true });

    for (var i = 0; i < 1e7; ++i) {
        if (i === 1e7 - 10)
            handler.set = undefined;
        proxy.foo = i; // shouldn't throw
    }

    shouldBe(setCalls, 1e7 - 10);
})();

shouldThrow(function() {
    "use strict";

    var target = {};
    var handler = { set: () => true };
    var proxy = new Proxy(target, handler);
    Object.defineProperty(target, "foo", { value: {}, writable: false, enumerable: true, configurable: true });

    for (var i = 0; i < 1e7; ++i) {
        if (i === 1e7 - 10)
            handler.set = null;
        proxy.foo = i;
    }
}, "TypeError: Attempted to assign to readonly property.");
