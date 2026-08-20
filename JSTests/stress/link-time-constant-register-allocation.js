// Link time constants are materialized by op_get_link_time_constant into a temporary rather than
// occupying a constant register. A temporary must not be allocated while a CallArguments block is
// live, and must not be stolen as a call's result register by finalDestination().

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

// @copyDataProperties is loaded while the rest-element CallArguments block is already reserved.
function objectRest(o) {
    const { x, ...rest } = o;
    return `${x}:${rest.y}:${rest.z}`;
}

// @cloneObject, reached through object spread.
function objectSpread(o) {
    return { ...o, w: 4 };
}

for (let i = 0; i < 1e5; ++i) {
    shouldBe(objectRest({ x: 1, y: 2, z: 3 }), "1:2:3");
    const spread = objectSpread({ x: 1, y: 2, z: 3 });
    shouldBe(spread.x + spread.y + spread.z + spread.w, 10);
    const both = objectSpread({ x: 1, y: 2, z: 3 });
    shouldBe(objectRest(both), "1:2:3");
}

// @createPrivateSymbol is called once per private name. With both an instance and a static private
// name the second call must not reuse the register still holding the first call's callee.
class C {
    #instance = 1;
    static #staticField = 2;
    #method() { return 3; }
    instance() { return this.#instance; }
    static staticField() { return C.#staticField; }
    method() { return this.#method(); }
}

for (let i = 0; i < 1e5; ++i) {
    const c = new C();
    shouldBe(c.instance(), 1);
    shouldBe(C.staticField(), 2);
    shouldBe(c.method(), 3);
}

// op_jneq_ptr / op_jeq_ptr carry a LinkTimeConstant operand. sentinelString is index 128, which does
// not fit a signed narrow operand, so these must still encode without silently widening.
function forIn(o) {
    let keys = "";
    for (const key in o)
        keys += key;
    return keys;
}
shouldBe(forIn({}), "");
shouldBe(forIn(null), "");
for (let i = 0; i < 1e5; ++i)
    shouldBe(forIn({ a: 1, b: 2 }), "ab");

// f.call / f.apply guards, and their negative paths.
function callee(a, b) { return this.base + a + b; }
const receiver = { base: 100 };
const impostor = function () { return "impostor"; };
const shadowed = function () { };
shadowed.call = impostor;
shadowed.apply = impostor;
for (let i = 0; i < 1e5; ++i) {
    shouldBe(callee.call(receiver, 1, 2), 103);
    shouldBe(callee.apply(receiver, [1, 2]), 103);
    shouldBe(shadowed.call(receiver, 1, 2), "impostor");
    shouldBe(shadowed.apply(receiver, [1, 2]), "impostor");
}
