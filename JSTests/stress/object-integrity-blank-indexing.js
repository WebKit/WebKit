function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try {
        func();
    } catch (e) {
        threw = true;
        if (!(e instanceof errorType) && e.name !== errorType.name)
            throw new Error("threw wrong error type: " + e);
    }
    if (!threw)
        throw new Error("did not throw");
}

// freeze / seal / preventExtensions on a plain (NonArray, blank indexing)
// object skips the eager ArrayStorage allocation; verify that no observable
// behavior changes.
for (let i = 0; i < testLoopCount; ++i) {
    {
        let o = Object.freeze({ a: 1, b: 2 });
        shouldBe(Object.isFrozen(o), true);
        shouldBe(Object.isSealed(o), true);
        shouldBe(Object.isExtensible(o), false);
        shouldBe(Object.getOwnPropertyDescriptor(o, "a").writable, false);
        shouldBe(Object.getOwnPropertyDescriptor(o, "a").configurable, false);

        // [[Set]] indexed: silently fail in sloppy, throw in strict.
        o[0] = 42;
        shouldBe(o[0], undefined);
        shouldBe(Reflect.set(o, 0, 42), false);
        shouldThrow(() => { "use strict"; o[5] = 42; }, TypeError);

        // [[DefineOwnProperty]] indexed: rejected.
        shouldThrow(() => Object.defineProperty(o, "0", { value: 1 }), TypeError);
        shouldBe(Reflect.defineProperty(o, "0", { value: 1 }), false);
        shouldBe(Object.getOwnPropertyNames(o).join(","), "a,b");
        shouldBe(Object.keys(o).join(","), "a,b");

        let keys = [];
        for (let k in o)
            keys.push(k);
        shouldBe(keys.join(","), "a,b");

        // [[Delete]] indexed: vacuously true.
        shouldBe(delete o[0], true);
    }
    {
        let o = Object.seal({ a: 1 });
        shouldBe(Object.isSealed(o), true);
        shouldBe(Object.isFrozen(o), false);
        shouldBe(Object.isExtensible(o), false);
        o.a = 2;
        shouldBe(o.a, 2);
        o[0] = 42;
        shouldBe(o[0], undefined);
        shouldThrow(() => { "use strict"; o[0] = 42; }, TypeError);
        shouldThrow(() => Object.defineProperty(o, "0", { value: 1 }), TypeError);
    }
    {
        let o = Object.preventExtensions({ a: 1 });
        shouldBe(Object.isExtensible(o), false);
        shouldBe(Object.isSealed(o), false);
        o[0] = 42;
        shouldBe(o[0], undefined);
        shouldThrow(() => { "use strict"; o[0] = 42; }, TypeError);
        shouldBe(Reflect.defineProperty(o, "0", { value: 1 }), false);
    }
    {
        let o = Object.freeze({});
        shouldBe(Object.isFrozen(o), true);
        shouldBe(Object.getOwnPropertyNames(o).length, 0);
        shouldThrow(() => { "use strict"; o[0] = 1; }, TypeError);
    }
    {
        let o = Object.freeze(Object.create(null, { a: { value: 1, enumerable: true } }));
        shouldBe(Object.isFrozen(o), true);
        let keys = [];
        for (let k in o)
            keys.push(k);
        shouldBe(keys.join(","), "a");
    }
}

// Invariant: enterDictionaryIndexingMode now leaves NonArray objects as
// NonArray. Any later indexed write must go through indexingShouldBeSparse()
// and lazily allocate ArrayStorage on demand (and then reject the write).
// Exercise every indexed-write entry point and verify the indexing mode at
// each step via $vm.indexingMode().
function indexingMode(o) { return $vm.indexingMode(o); }
noInline(indexingMode);

function shouldBeNonArray(o) { shouldBe(indexingMode(o), "NonArray"); }
function shouldBeArrayStorage(o) {
    // --alwaysHaveABadTime=true (e.g. lockdown) yields SlowPutArrayStorage.
    let mode = indexingMode(o);
    if (mode !== "NonArrayWithArrayStorage" && mode !== "NonArrayWithSlowPutArrayStorage")
        throw new Error("bad value: " + mode + " expected: NonArrayWith{SlowPut,}ArrayStorage");
}

for (let make of [() => Object.freeze({ a: 1 }), () => Object.seal({ a: 1 }), () => Object.preventExtensions({ a: 1 }), () => Object.freeze({})]) {
    for (let i = 0; i < testLoopCount; ++i) {
        // Immediately after the integrity-level transition, no indexing storage.
        shouldBeNonArray(make());

        // putByIndex (sloppy): lazily enters dictionary indexing, write is rejected.
        {
            let o = make();
            o[0] = 42;
            shouldBeArrayStorage(o);
            shouldBe(o[0], undefined);
            shouldBe(0 in o, false);
            // Second write on the same object must keep being rejected.
            o[1] = 42;
            shouldBe(o[1], undefined);
        }
        // putByIndex (strict): lazily enters dictionary indexing, throws.
        {
            let o = make();
            shouldThrow(() => { "use strict"; o[0] = 42; }, TypeError);
            shouldBeArrayStorage(o);
            shouldBe(0 in o, false);
        }
        // putByIndex via Reflect.set.
        {
            let o = make();
            shouldBe(Reflect.set(o, 5, 42), false);
            shouldBeArrayStorage(o);
            shouldBe(5 in o, false);
        }
        // putByIndex via Array.prototype.push (generic [[Set]] on a non-array).
        {
            let o = make();
            shouldThrow(() => { "use strict"; Array.prototype.push.call(o, 42); }, TypeError);
            shouldBe(0 in o, false);
        }
        // Large index (sparse path).
        {
            let o = make();
            o[0xFFFFFFFE] = 42;
            shouldBeArrayStorage(o);
            shouldBe(0xFFFFFFFE in o, false);
        }
        // defineOwnIndexedProperty: data descriptor.
        {
            let o = make();
            shouldThrow(() => Object.defineProperty(o, "0", { value: 42 }), TypeError);
            shouldBeArrayStorage(o);
            shouldBe(0 in o, false);
        }
        // defineOwnIndexedProperty: data descriptor with attributes.
        {
            let o = make();
            shouldBe(Reflect.defineProperty(o, "0", { value: 42, writable: false, configurable: false }), false);
            shouldBeArrayStorage(o);
            shouldBe(0 in o, false);
        }
        // defineOwnIndexedProperty: accessor descriptor.
        {
            let o = make();
            shouldBe(Reflect.defineProperty(o, "0", { get() { return 42; } }), false);
            shouldBeArrayStorage(o);
            shouldBe(0 in o, false);
        }
        // putDirectIndex via Object.defineProperties.
        {
            let o = make();
            shouldThrow(() => Object.defineProperties(o, { 0: { value: 42 } }), TypeError);
            shouldBe(0 in o, false);
        }
        // Read paths must not allocate storage.
        {
            let o = make();
            shouldBe(o[0], undefined);
            shouldBe(0 in o, false);
            shouldBe(Object.getOwnPropertyDescriptor(o, "0"), undefined);
            shouldBe(delete o[0], true);
            shouldBeNonArray(o);
        }
        // for-in / Object.keys must not allocate storage and must use the cache.
        {
            let o = make();
            let keys = [];
            for (let k in o)
                keys.push(k);
            shouldBe(keys.indexOf("0"), -1);
            Object.keys(o);
            Object.getOwnPropertyNames(o);
            shouldBeNonArray(o);
        }
        // Object.isFrozen / isSealed must not allocate storage (fast path).
        {
            let o = make();
            Object.isFrozen(o);
            Object.isSealed(o);
            Object.isExtensible(o);
            shouldBeNonArray(o);
        }
    }
}

// Setting an indexed property on a child whose prototype is frozen+blank must
// succeed on the child (the rejection is per-receiver, not per-prototype).
{
    let proto = Object.freeze({ a: 1 });
    shouldBeNonArray(proto);
    let child = Object.create(proto);
    child[0] = 42;
    shouldBe(child[0], 42);
    shouldBeNonArray(proto);
    shouldBe(0 in proto, false);
}

// ArrayClass (the other ALL_BLANK_INDEXING_TYPES case, e.g. Array.prototype)
// is intentionally NOT short-circuited; JSArray code paths such as
// setLengthWritable/pushInline assume enterDictionaryIndexingMode allocated
// ArrayStorage. Guard against regressing that. Use a fresh realm so we don't
// poison this realm's Array.prototype.
{
    let other = $vm.createGlobalObject();
    let proto = other.Array.prototype;
    shouldBe(other.Array.isArray(proto), true);
    Object.preventExtensions(proto);
    shouldThrow(() => { "use strict"; proto.push(1); }, TypeError);
    shouldBe(proto.length, 0);
}
{
    let other = $vm.createGlobalObject();
    let proto = other.Array.prototype;
    Object.defineProperty(proto, "length", { writable: false });
    shouldBe(Object.getOwnPropertyDescriptor(proto, "length").writable, false);
    shouldThrow(() => { "use strict"; proto.length = 5; }, TypeError);
    shouldBe(proto.length, 0);
}
