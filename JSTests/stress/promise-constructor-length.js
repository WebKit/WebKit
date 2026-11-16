// The Promise constructor's "length" is no longer defined eagerly at creation time; it is
// lazily reified from the builtin executable's parameter count. This locks down the
// observable shape: value 1 with { writable: false, enumerable: false, configurable: true },
// correct property ordering, and standard delete / redefine behavior.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

shouldBe(Promise.length, 1);
shouldBe(Object.prototype.hasOwnProperty.call(Promise, "length"), true);

let desc = Object.getOwnPropertyDescriptor(Promise, "length");
shouldBe(desc.value, 1);
shouldBe(desc.writable, false);
shouldBe(desc.enumerable, false);
shouldBe(desc.configurable, true);

// "length" and "name" must come first in own property order.
let keys = Object.getOwnPropertyNames(Promise);
shouldBe(keys[0], "length");
shouldBe(keys[1], "name");
shouldBe(Object.keys(Promise).length, 0);

// writable: false. Strict mode write throws; the value is unchanged.
shouldBe((() => {
    "use strict";
    try {
        Promise.length = 42;
        return "no throw";
    } catch (error) {
        return String(error.constructor === TypeError);
    }
})(), "true");
shouldBe(Promise.length, 1);

// Bound function lengths derive from the original length.
shouldBe(Promise.bind(null).length, 1);
shouldBe(Promise.bind(null, function() {}).length, 0);

// configurable: true. delete and redefine work.
shouldBe(delete Promise.length, true);
shouldBe(Object.prototype.hasOwnProperty.call(Promise, "length"), false);
shouldBe(Promise.length, 0); // Inherited from Function.prototype.length.

Object.defineProperty(Promise, "length", { value: 7, writable: false, enumerable: false, configurable: true });
shouldBe(Promise.length, 7);
