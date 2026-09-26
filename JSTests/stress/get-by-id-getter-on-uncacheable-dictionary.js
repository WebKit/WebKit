function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function makeExports() {
    const exports = {};
    for (let i = 0; i < 100; ++i)
        exports["name" + i] = undefined;
    for (let i = 0; i < 100; ++i)
        Object.defineProperty(exports, "name" + i, { enumerable: true, configurable: true, get() { return i; } });
    return exports;
}

function own(object) {
    return object.name70;
}
noInline(own);

function inherited(object) {
    return object.name70;
}
noInline(inherited);

function ownByVal(object, key) {
    return object[key];
}
noInline(ownByVal);

{
    const exports = makeExports();
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(own(exports), 70);
        shouldBe(ownByVal(exports, "name99"), 99);
    }

    Object.defineProperty(exports, "name70", { value: "data" });
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(own(exports), "data");

    Object.defineProperty(exports, "name70", { get() { return "accessor"; } });
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(own(exports), "accessor");

    delete exports.name70;
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(own(exports), undefined);

    for (let i = 0; i < 100; ++i)
        Object.defineProperty(exports, "name" + i, { configurable: true, get() { return -i; } });
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(own(exports), -70);
        shouldBe(ownByVal(exports, "name99"), -99);
    }
}

{
    const exports = makeExports();
    const derived = Object.create(exports);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(inherited(derived), 70);

    Object.defineProperty(exports, "name70", { value: "data" });
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(inherited(derived), "data");

    Object.defineProperty(derived, "name70", { value: "shadow" });
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(inherited(derived), "shadow");
}
