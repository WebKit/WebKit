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

// Object.create reuses one Structure per prototype object, so the objects built after the freeze
// carry the same StructureIDs that warmed the store cache and can still hit its entries. Freezing
// the receiver instead would hand it a fresh StructureID and prove nothing.
function warmUp(put, proto) {
    for (let i = 0; i < testLoopCount; ++i)
        put(makeShape(proto, i % shapeCount), i);
    Object.freeze(proto);
}

{
    const proto = { x: "protoValue" };
    function put(object, value) { "use strict"; object.x = value; }
    noInline(put);
    warmUp(put, proto);
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(proto, i);
        shouldThrow(() => put(object, -1), `TypeError: Attempted to assign to readonly property.`);
        shouldBe(object.x, "protoValue");
        shouldBe(Object.getOwnPropertyDescriptor(object, "x"), undefined);
    }
}

{
    const proto = { get other() { return 1; }, x: "protoValue" };
    function put(object, value) { "use strict"; object.x = value; }
    noInline(put);
    warmUp(put, proto);
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(proto, i);
        shouldThrow(() => put(object, -1), `TypeError: Attempted to assign to readonly property.`);
        shouldBe(object.x, "protoValue");
        shouldBe(Object.getOwnPropertyDescriptor(object, "x"), undefined);
    }
}

{
    const proto = { get other() { return 1; }, x: "protoValue" };
    function put(object, value) { object.x = value; }
    noInline(put);
    warmUp(put, proto);
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(proto, i);
        put(object, -1);
        shouldBe(object.x, "protoValue");
        shouldBe(Object.getOwnPropertyDescriptor(object, "x"), undefined);
    }
}

{
    const proto = { get other() { return 1; }, x: "protoValue" };
    function put(object, key, value) { "use strict"; object[key] = value; }
    noInline(put);
    for (let i = 0; i < testLoopCount; ++i)
        put(makeShape(proto, i % shapeCount), "x", i);
    Object.freeze(proto);
    for (let i = 0; i < shapeCount; ++i) {
        const object = makeShape(proto, i);
        shouldThrow(() => put(object, "x", -1), `TypeError: Attempted to assign to readonly property.`);
        shouldBe(object.x, "protoValue");
        shouldBe(Object.getOwnPropertyDescriptor(object, "x"), undefined);
    }
}
