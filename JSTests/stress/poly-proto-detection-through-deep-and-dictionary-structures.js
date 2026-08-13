function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

function isPolyProto(object) {
    return /PolyProto offset/.test(describe(object));
}

function isDictionary(object) {
    return /Dictionary/.test(describe(object));
}

function test(numberOfProperties, expectDictionary) {
    let makeInstance = new Function(`
        function C() {
            for (let i = 0; i < ${numberOfProperties}; ++i)
                this["p" + i] = i;
        }
        return new C;
    `);
    function access(object) { return object.p3; }

    shouldBe(isDictionary(makeInstance()), expectDictionary);

    for (let i = 0; i < 10; ++i) {
        let object = makeInstance();
        shouldBe(access(object), 3);
        shouldBe(access(object), 3);
    }
    return isPolyProto(makeInstance());
}

// Poly proto opportunities are only detected by inline caches.
let expected = !!(jscOptions().forcePolyProto || jscOptions().useJIT || jscOptions().useLLIntICs);
shouldBe(test(100, false), expected);
shouldBe(test(200, true), expected);
