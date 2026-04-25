function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

function makeDictionary(dict) {
    for (let i = 0; i < 1000; ++i)
        dict["k" + i] = i;
    return dict;
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() {
                this._field = 42;
            }
        }
        return new C();
    }
    for (let i = 0; i < 15; ++i)
        foo();
    return foo();
}

function test(proto, key1, key2) {
    proto[key1] = 1;
    proto[key2] = 2;
    Object.defineProperty(proto, key2, {enumerable: false});

    const child = Object.create(proto);

    function isKey1Enumerable(obj) {
        for (let key in obj) {
            if (key == key1) return true;
        }
        return false;
    }

    function isKey2Enumerable(obj) {
        for (let key in obj) {
            if (key == key2) return true;
        }
        return false;
    }

    noInline(isKey1Enumerable);
    noInline(isKey2Enumerable);

    for (let i = 0; i < 20; ++i) {
        shouldBe(isKey1Enumerable(child), true);
        shouldBe(isKey1Enumerable(proto), true);
        shouldBe(isKey2Enumerable(child), false);
        shouldBe(isKey2Enumerable(proto), false);
    }

    Object.defineProperty(proto, key1, {enumerable: false});
    shouldBe(isKey1Enumerable(child), false);
    shouldBe(isKey1Enumerable(proto), false);

    Object.defineProperty(proto, key2, {enumerable: true});
    shouldBe(isKey2Enumerable(child), true);
    shouldBe(isKey2Enumerable(proto), true);
}

test({}, 0, "bar");
test(Object.create(null), "__proto__", 1);

test(makeDictionary({}), "foo", 2);
test(makeDictionary(Object.create(null)), 3, "__proto__");

test(makePolyProtoObject(), "foo", "bar");
test(makePolyProtoObject(), 4, 5);
