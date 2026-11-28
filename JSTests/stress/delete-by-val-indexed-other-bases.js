//@ requireOptions("--useDollarVM=1")

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + String(actual) + " expected: " + String(expected));
}

function makeSloppy() {
    const remove = new Function("object", "key", "return delete object[key];");
    noInline(remove);
    return remove;
}

function makeStrict() {
    const remove = new Function("object", "key", "'use strict'; return delete object[key];");
    noInline(remove);
    return remove;
}

const sharedSloppy = makeSloppy();
const sharedStrict = makeStrict();

function attempt(remove, object, key) {
    try {
        return remove(object, key);
    } catch (error) {
        if (!(error instanceof TypeError))
            throw error;
        return "throws";
    }
}

function testRefused(object, key, value) {
    for (const [remove, expected] of [[makeSloppy(), false], [makeStrict(), "throws"], [sharedSloppy, false], [sharedStrict, "throws"]]) {
        // Throwing is slow, and what happens before it is the same in both modes.
        const count = expected === "throws" ? testLoopCount / 20 : testLoopCount / 4;
        for (let i = 0; i < count; ++i) {
            shouldBe(attempt(remove, object, key), expected);
            shouldBe(object[key], value);
        }
    }
}

testRefused(Object.freeze([0, 1, 2]), 1, 1);
testRefused(Object.seal({ 0: 0, 1: 1 }), 1, 1);
testRefused(Object.defineProperty([0, 1, 2], 1, { configurable: false }), 1, 1);
testRefused(Object.defineProperty({ 0: 0 }, 1, { get() { return 42; } }), 1, 42);
testRefused(new Uint8Array([0, 1, 2]), 1, 1);
testRefused(new String("abc"), 1, "b");

function testAllowed(create, key, value) {
    for (const remove of [makeSloppy(), makeStrict(), sharedSloppy, sharedStrict]) {
        for (let i = 0; i < testLoopCount / 10; ++i) {
            const object = create();
            shouldBe(object[key], value);
            shouldBe(remove(object, key), true);
            shouldBe(key in object, false);
        }
    }
}

class DerivedArray extends Array { }
testAllowed(() => new DerivedArray(0, 1, 2), 1, 1);
testAllowed(() => (function () { return arguments; })(0, 1, 2), 1, 1);
testAllowed(() => Object.preventExtensions([0, 1, 2]), 1, 1);
testAllowed(() => new Uint8Array(2), 2, undefined);
testAllowed(() => ({ 0: 0, [-1]: "minus one" }), -1, "minus one");
testAllowed(() => ({ 0: 0, [-2147483648]: "min" }), -2147483648, "min");

{
    let trapped = 0;
    const proxy = new Proxy([0, 1, 2], { deleteProperty(target, key) { ++trapped; shouldBe(key, "1"); return true; } });
    const remove = makeStrict();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe((i & 1 ? remove : sharedStrict)(proxy, 1), true);
    shouldBe(trapped, testLoopCount);
    shouldBe(proxy[1], 1);
}

{
    // This is an array whose type is not ArrayType.
    const array = $vm.createRuntimeArray(0, 1, 2);
    const expected = Reflect.deleteProperty(array, 1);
    const remove = makeSloppy();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe((i & 1 ? remove : sharedSloppy)(array, 1), expected);
    shouldBe(array[1], 1);
}

{
    const remove = makeSloppy();
    for (let i = 0; i < testLoopCount; ++i) {
        const array = [i, 1, 2, 3];
        array.length = 2;
        shouldBe(remove(array, 3), true);
        shouldBe(array.length, 2);
        array.length = 4;
        shouldBe(3 in array, false);
    }
}

{
    const remove = makeSloppy();
    const symbol = Symbol();
    const keys = [0, "0", 0.5, -0, symbol, "named", 4294967295, 2147483648];
    for (let i = 0; i < testLoopCount; ++i) {
        const key = keys[i % keys.length];
        const object = { 0: 0, 0.5: 1, [symbol]: 2, named: 3, 4294967295: 4, 2147483648: 5 };
        shouldBe(remove(object, key), true);
        shouldBe(key in object, false);
        shouldBe(Reflect.ownKeys(object).length, 5);
    }
}

{
    const remove = makeStrict();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(remove({ 0: i }, 0), true);
    shouldBe(remove(42, 0), true);
    shouldBe(attempt(remove, null, 0), "throws");
    shouldBe(attempt(remove, "abc", 0), "throws");
    shouldBe(remove(Symbol.iterator, 0), true);
}

for (const namedFirst of [true, false]) {
    const remove = makeStrict();
    const named = [() => ({ 0: 1, name: 1 }), "name"];
    const indexed = [
        [() => [0, 1, {}], 1],
        [() => { const array = []; array.push(0.5, 1.5); return array; }, 1],
        [() => ({ 0: 1, 1: 2 }), 1],
        [() => { const array = [0, 1]; $vm.ensureArrayStorage(array); return array; }, 1],
        [() => [0, 1], 1],
        [() => ({ 0: 1 }), 1000],
        [() => ({ 0: 1, [-5]: 1 }), -5],
    ];
    const cases = namedFirst ? [named, ...indexed] : [...indexed, named];
    for (const oneByOne of [true, false]) {
        for (let i = 0; i < testLoopCount / 2; ++i) {
            const [create, key] = oneByOne ? cases[Math.floor(i / (testLoopCount / 2) * cases.length)] : cases[i % cases.length];
            const object = create();
            const count = Reflect.ownKeys(object).filter(k => k !== String(key)).length;
            shouldBe(remove(object, key), true);
            shouldBe(key in object, false);
            shouldBe(Reflect.ownKeys(object).length, count);
        }
    }
}

// An index is what makes the IC give up.
for (const [remove, refused] of [[makeSloppy(), false], [makeStrict(), "throws"]]) {
    const createIndexed = [() => ({ 0: 1 }), () => ({ 0: 1.5 }), () => ({ 0: "a" })];
    for (let i = 0; i < testLoopCount; ++i) {
        const named = { };
        if (i % 9 < 6)
            named["x" + i % 9] = 1;
        const [object, key] = i < testLoopCount / 2 ? [named, "y"] : [createIndexed[i % 3](), 0];
        shouldBe(remove(object, key), true);
        if (!(i % 10))
            shouldBe(attempt(remove, Object.freeze([1, 2]), 1), refused);
    }
}
