"use strict";

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

const shapeCount = 64;

function makeShape(proto, i) {
    const object = Object.create(proto);
    for (let j = 0; j <= (i % 8); ++j)
        object["k" + ((i * 5 + j * 3) % 13)] = j;
    object["tag" + i] = i;
    return object;
}

// Builds P1 -> P2 where P1 shadows P2's "x" with a writable data property, and
// turns P1 into an uncacheable dictionary so that deleting P1.x later removes
// the property in place without a Structure transition.
function makeChain(P2) {
    const P1 = Object.create(P2);
    Object.defineProperty(P1, "x", { value: 0, writable: true, configurable: true, enumerable: true });
    for (let i = 0; i < 5000; ++i)
        P1["p" + i] = i;
    for (let i = 0; i < testLoopCount; ++i)
        makeShape(P1, i % shapeCount);
    delete P1.p0;
    return P1;
}

function put(object, value) {
    object.x = value;
}
noInline(put);

const unrelated = { unrelated: 1 };
for (let i = 0; i < testLoopCount; ++i)
    put(makeShape(unrelated, i % shapeCount), i);

{
    let setterCalls = 0;
    const P2 = { set x(value) { setterCalls++; } };
    const P1 = makeChain(P2);
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(P1, i % shapeCount);
        put(object, i);
        shouldBe(Object.hasOwn(object, "x"), true);
    }
    shouldBe(setterCalls, 0);

    delete P1.x;
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(P1, i);
        put(object, i);
        shouldBe(Object.hasOwn(object, "x"), false);
    }
    shouldBe(setterCalls, shapeCount);
}

{
    const P2 = Object.defineProperty({}, "x", { value: 42, writable: false, configurable: true, enumerable: true });
    const P1 = makeChain(P2);
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(P1, i % shapeCount);
        put(object, i);
        shouldBe(object.x, i);
    }

    delete P1.x;
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(P1, i);
        shouldThrow(() => put(object, i), "TypeError: Attempted to assign to readonly property.");
        shouldBe(Object.hasOwn(object, "x"), false);
        shouldBe(object.x, 42);
    }
}
