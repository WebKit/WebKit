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

    const child = Object.create(proto);

    function getKey1(obj) {
        return obj[key1];
    }

    function getKey2(obj) {
        return obj[key2];
    }

    noInline(getKey1);
    noInline(getKey2);

    for (let i = 0; i < 1e4; ++i) {
        shouldBe(getKey1(child), 1);
        shouldBe(getKey1(proto), 1);
        shouldBe(getKey2(child), 2);
        shouldBe(getKey2(proto), 2);
    }

    Object.defineProperty(proto, key1, {value: 3});
    shouldBe(getKey1(child), 3);
    shouldBe(getKey1(proto), 3);

    Object.defineProperty(proto, key2, {set() {}});
    shouldBe(getKey2(child), undefined);
    shouldBe(getKey2(proto), undefined);
}

test({}, 0, "bar");
test(Object.create(null), "__proto__", 1);

test(makeDictionary({}), "foo", 2);
test(makeDictionary(Object.create(null)), 3, "__proto__");

test(makePolyProtoObject(), "foo", "bar");
test(makePolyProtoObject(), 4, 5);
