//@ requireOptions("--useJointIteration=1")

// Test cases adapted from the Iterator.zip / Iterator.zipKeyed examples in the TC39 Joint Iteration proposal repository.

function stringify(value) {
    if (typeof value === "string")
        return JSON.stringify(value);
    if (typeof value !== "object" || value === null)
        return String(value);
    if (Array.isArray(value))
        return "[" + value.map(stringify).join(", ") + "]";
    var proto = Object.getPrototypeOf(value) === null ? ", __proto__: null" : "";
    return "{" + Reflect.ownKeys(value).map((key) => `${String(key)}: ${stringify(value[key])}`).join(", ") + proto + "}";
}

function deepEqual(actual, expected) {
    if (Object.is(actual, expected))
        return true;

    if (typeof actual !== "object" || actual === null || typeof expected !== "object" || expected === null)
        return false;

    if (Object.getPrototypeOf(actual) !== Object.getPrototypeOf(expected))
        return false;

    if (Array.isArray(expected)) {
        if (!Array.isArray(actual) || actual.length !== expected.length)
            return false;
        for (var i = 0; i < expected.length; ++i) {
            if (!deepEqual(actual[i], expected[i]))
                return false;
        }
        return true;
    }

    var actualKeys = Reflect.ownKeys(actual);
    var expectedKeys = Reflect.ownKeys(expected);
    if (actualKeys.length !== expectedKeys.length)
        return false;
    for (var key of expectedKeys) {
        if (!Object.prototype.hasOwnProperty.call(actual, key) || !deepEqual(actual[key], expected[key]))
            return false;
    }
    return true;
}

function shouldBe(label, actual, expected) {
    if (!deepEqual(actual, expected))
        throw new Error(`FAIL (${label}): got ${stringify(actual)}, expected ${stringify(expected)}`);
}

function shouldThrow(label, callback, errorConstructor) {
    try {
        callback();
    } catch (e) {
        if (!(e instanceof errorConstructor))
            throw new Error(`FAIL (${label}): expected ${errorConstructor.name}, got ${e}`);
        return;
    }
    throw new Error(`FAIL (${label}): should have thrown ${errorConstructor.name}`);
}

shouldBe("positional shortest", Array.from(Iterator.zip([
    [0],
    [1, 2],
])), [
    [0, 1],
]);

shouldBe("positional equiv", Array.from(Iterator.zip([
    [0, 1, 2],
    [3, 4, 5],
    [6, 7, 8],
])), [
    [0, 3, 6],
    [1, 4, 7],
    [2, 5, 8],
]);

shouldBe("positional empty", Array.from(Iterator.zip([])), []);

shouldBe("positional longest", Array.from(Iterator.zip([
    [0],
    [1, 2],
], { mode: "longest" })), [
    [0, 1],
    [undefined, 2],
]);

shouldThrow("positional strict", () => {
    Array.from(Iterator.zip([
        [0],
        [1, 2],
    ], { mode: "strict" }));
}, TypeError);

shouldBe("named shortest", Array.from(Iterator.zipKeyed({
    a: [0],
    b: [1, 2],
})), [
    { a: 0, b: 1, __proto__: null },
]);

shouldBe("named equiv", Array.from(Iterator.zipKeyed({
    a: [0, 1, 2],
    b: [3, 4, 5],
    c: [6, 7, 8],
})), [
    { a: 0, b: 3, c: 6, __proto__: null },
    { a: 1, b: 4, c: 7, __proto__: null },
    { a: 2, b: 5, c: 8, __proto__: null },
]);

shouldBe("named empty", Array.from(Iterator.zipKeyed({})), []);

shouldBe("named longest", Array.from(Iterator.zipKeyed({
    a: [0],
    b: [1, 2],
}, { mode: "longest" })), [
    { a: 0, b: 1, __proto__: null },
    { a: undefined, b: 2, __proto__: null },
]);

shouldThrow("named strict", () => {
    Array.from(Iterator.zipKeyed({
        a: [0],
        b: [1, 2],
    }, { mode: "strict" }));
}, TypeError);

{
    const padding = [{}, {}, {}, {}];

    shouldBe("padding positional 2", Array.from(Iterator.zip([
        [0],
        [1, 2, 3],
    ], { mode: "longest", padding })), [
        [0, 1],
        [padding[0], 2],
        [padding[0], 3],
    ]);

    shouldBe("padding positional 4", Array.from(Iterator.zip([
        [0],
        [1, 2, 3],
        [4, 5],
        [],
    ], { mode: "longest", padding })), [
        [0, 1, 4, padding[3]],
        [padding[0], 2, 5, padding[3]],
        [padding[0], 3, padding[2], padding[3]],
    ]);
}

{
    const A_PADDING = {};
    const B_PADDING = {};
    const C_PADDING = {};
    const D_PADDING = {};

    const padding = {
        a: A_PADDING,
        b: B_PADDING,
        c: C_PADDING,
        d: D_PADDING,
    };

    shouldBe("padding named 2", Array.from(Iterator.zipKeyed({
        a: [0],
        b: [1, 2, 3],
    }, { mode: "longest", padding })), [
        { a: 0, b: 1, __proto__: null },
        { a: A_PADDING, b: 2, __proto__: null },
        { a: A_PADDING, b: 3, __proto__: null },
    ]);

    shouldBe("padding named 4", Array.from(Iterator.zipKeyed({
        a: [0],
        b: [1, 2, 3],
        c: [4, 5],
        d: [],
    }, { mode: "longest", padding })), [
        { a: 0, b: 1, c: 4, d: D_PADDING, __proto__: null },
        { a: A_PADDING, b: 2, c: 5, d: D_PADDING, __proto__: null },
        { a: A_PADDING, b: 3, c: C_PADDING, d: D_PADDING, __proto__: null },
    ]);
}
