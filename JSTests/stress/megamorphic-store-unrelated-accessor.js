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

{
    const proto = { get other() { return 1; } };
    function put(object, value) {
        object.x = value;
    }
    noInline(put);
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(proto, i % shapeCount);
        object.x = 0;
        put(object, i);
        shouldBe(object.x, i);
    }
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(proto, i % shapeCount);
        put(object, i);
        shouldBe(object.x, i);
        shouldBe(Object.getOwnPropertyDescriptor(object, "x").writable, true);
    }
}

{
    const proto = {};
    function put(object, value) {
        object.def = value;
    }
    noInline(put);
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(proto, i % shapeCount);
        Object.defineProperty(object, "lazy", { get() { return "lazy"; } });
        put(object, i);
        shouldBe(object.def, i);
        shouldBe(object.lazy, "lazy");
    }
}

{
    const proto = { get other() { return 1; } };
    function put(object, key, value) {
        object[key] = value;
    }
    noInline(put);
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(proto, i % shapeCount);
        put(object, "x", i);
        shouldBe(object.x, i);
    }
}

{
    let setterCalls = 0;
    const plain = { get other() { return 1; } };
    const withSetter = {
        get other() { return 1; },
        set x(value) {
            ++setterCalls;
            this._x = value;
        },
    };
    const withReadOnly = { get other() { return 1; } };
    Object.defineProperty(withReadOnly, "x", { value: "readonly", writable: false });
    function put(object, value) {
        object.x = value;
    }
    noInline(put);
    for (let i = 0; i < testLoopCount; ++i)
        put(makeShape(plain, i % shapeCount), i);
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(withSetter, i % shapeCount);
        put(object, i);
        shouldBe(object._x, i);
        shouldBe(Object.getOwnPropertyDescriptor(object, "x"), undefined);
    }
    shouldBe(setterCalls, testLoopCount);
    for (let i = 0; i < testLoopCount; ++i) {
        const object = makeShape(withReadOnly, i % shapeCount);
        shouldThrow(() => put(object, i), `TypeError: Attempted to assign to readonly property.`);
        shouldBe(object.x, "readonly");
    }
}

{
    const proto = { get other() { return 1; } };
    function put(object, value) {
        object.x = value;
    }
    noInline(put);
    for (let i = 0; i < testLoopCount; ++i)
        put(makeShape(proto, i % shapeCount), i);
    let setterCalls = 0;
    Object.defineProperty(proto, "x", { set(value) { ++setterCalls; }, configurable: true });
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(proto, i);
        put(object, i);
        shouldBe(Object.getOwnPropertyDescriptor(object, "x"), undefined);
    }
    shouldBe(setterCalls, shapeCount);
}

{
    const proto = { get other() { return 1; } };
    function put(object, value) {
        object.x = value;
    }
    noInline(put);
    const objects = [];
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(proto, i);
        object.x = 0;
        objects.push(object);
    }
    for (let i = 0; i < testLoopCount; ++i)
        put(objects[i % shapeCount], i);
    for (const object of objects) {
        const before = object.x;
        Object.freeze(object);
        shouldThrow(() => put(object, -1), `TypeError: Attempted to assign to readonly property.`);
        shouldBe(object.x, before);
    }
}
