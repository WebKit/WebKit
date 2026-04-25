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
    Object.defineProperty(proto, key2, {writable: false});

    const child = Object.create(proto);
    let nextValue = 10;

    function isKey1WritableStrict(obj) {
        "use strict";
        try {
            obj[key1] = ++nextValue;
        } catch {
            return false;
        }
        return true;
    }

    function isKey1Writable(obj) {
        obj[key1] = ++nextValue;
        return obj[key1] === nextValue;
    }

    function isKey2WritableStrict(obj) {
        "use strict";
        try {
            obj[key2] = ++nextValue;
        } catch {
            return false;
        }
        return true;
    }

    function isKey2Writable(obj) {
        obj[key2] = ++nextValue;
        return obj[key2] === nextValue;
    }

    noInline(isKey1WritableStrict);
    noInline(isKey1Writable);
    noInline(isKey2WritableStrict);
    noInline(isKey2Writable);

    for (let i = 0; i < 1e4; ++i) {
        shouldBe(isKey1WritableStrict(child), true);
        shouldBe(isKey1Writable(proto), true);
        shouldBe(isKey2WritableStrict(child), false);
        shouldBe(isKey2Writable(proto), false);

        delete child[key1];
        delete child[key2];
    }

    Object.defineProperty(proto, key1, {writable: false});
    shouldBe(isKey1WritableStrict(child), false);
    shouldBe(isKey1Writable(proto), false);

    Object.defineProperty(proto, key2, {writable: true});
    shouldBe(isKey2WritableStrict(child), true);
    shouldBe(isKey2Writable(proto), true);
}

test({}, 0, "bar");
test(Object.create(null), "__proto__", 1);

test(makeDictionary({}), "foo", 2);
test(makeDictionary(Object.create(null)), 3, "__proto__");

test(makePolyProtoObject(), "foo", "bar");
test(makePolyProtoObject(), 4, 5);
