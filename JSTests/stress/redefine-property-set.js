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
    let nextValue = 10;

    let value1;
    Object.defineProperty(proto, key1, {
        set(val) { value1 = val; },
        configurable: true,
    });

    let value2;
    Object.defineProperty(proto, key2, {
        set(val) { value2 = val; },
        configurable: true,
    });

    const child = Object.create(proto);

    function setKey1(obj) {
        obj[key1] = ++nextValue;
        return value1 === nextValue;
    }

    function setKey2(obj) {
        obj[key2] = ++nextValue;
        return value2 === nextValue;
    }

    noInline(setKey1);
    noInline(setKey2);

    for (let i = 0; i < 1e4; ++i) {
        shouldBe(setKey1(child), true);
        shouldBe(setKey1(proto), true);
        shouldBe(setKey2(child), true);
        shouldBe(setKey2(proto), true);
    }

    Object.defineProperty(proto, key1, {set: undefined});
    shouldBe(setKey1(child), false);
    shouldBe(setKey1(proto), false);

    Object.defineProperty(proto, key2, {set() {}});
    shouldBe(setKey2(child), false);
    shouldBe(setKey2(proto), false);
}

test({}, 0, "bar");
test(Object.create(null), "__proto__", 1);

test(makeDictionary({}), "foo", 2);
test(makeDictionary(Object.create(null)), 3, "__proto__");

test(makePolyProtoObject(), "foo", "bar");
test(makePolyProtoObject(), 4, 5);
