function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; i++)
        shouldBe(actual[i], expected[i]);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`expected ${errorType.name} but got ${error}`);
}

let functionCount = 0;

function makeSet(body = "return target.set(source, offset);") {
    const set = new Function("target", "source", "offset", `${body} // ${functionCount++}`);
    noInline(set);
    return set;
}

function values(constructor, array) {
    if (constructor === BigInt64Array || constructor === BigUint64Array)
        return array.map(BigInt);
    return array;
}

function warmUp(set, constructor) {
    const source = new constructor(values(constructor, [1, 2, 3, 4]));
    const target = new constructor(8);
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(set(target, source, i & 3), undefined);
    shouldBe(set(target, source, 4), undefined);
    shouldBeArray(target.subarray(4), values(constructor, [1, 2, 3, 4]));
    return set;
}

function testCopy(constructor) {
    const set = warmUp(makeSet(), constructor);
    const source = new constructor(values(constructor, [1, 2, 3, 4]));
    const target = new constructor(8);
    const zero = values(constructor, [0])[0];
    for (let offset = 0; offset <= 4; offset++) {
        target.fill(zero);
        shouldBe(set(target, source, offset), undefined);
        for (let j = 0; j < 8; j++)
            shouldBe(target[j], j >= offset && j < offset + 4 ? source[j - offset] : zero);
    }
    shouldThrow(() => set(target, source, 5), RangeError);
    shouldThrow(() => set(target, source, -1), RangeError);
}

function testWithoutOffset() {
    const set = makeSet("return target.set(source);");
    const source = new Float64Array([NaN, -0, Infinity, 1.5]);
    const target = new Float64Array(8);
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(set(target, source), undefined);
    shouldBe(Object.is(target[0], NaN), true);
    shouldBe(Object.is(target[1], -0), true);
    shouldBe(target[2], Infinity);
    shouldBe(target[3], 1.5);
    shouldBe(target[4], 0);
    shouldThrow(() => set(source, target), RangeError);
}

function testExtraArguments() {
    const set = makeSet("return target.set(source, offset, 1, 2);");
    const source = new Uint8Array([1, 2]);
    const target = new Uint8Array(4);
    for (let i = 0; i < testLoopCount; i++)
        set(target, source, 2);
    shouldBeArray(target, [0, 0, 1, 2]);
}

function testTailCall() {
    const set = warmUp(makeSet("'use strict'; return target.set(source, offset);"), Uint8Array);
    const source = new Uint8Array([1, 2, 3, 4]);
    const target = new Uint8Array(8);
    shouldThrow(() => set(target, source, 5), RangeError);
    shouldBe(set(target, source, 4), undefined);
    shouldBeArray(target, [0, 0, 0, 0, 1, 2, 3, 4]);
}

function testInlined() {
    function inlined(target, source, offset) {
        return target.set(source, offset);
    }
    function caller(target, source, offset) {
        return inlined(target, source, offset);
    }
    noInline(caller);
    warmUp(caller, Uint16Array);
    const source = new Uint16Array([1, 2, 3, 4]);
    const target = new Uint16Array(8);
    shouldThrow(() => caller(target, source, 5), RangeError);
    warmUp(caller, Uint16Array);
    shouldBe(caller(target, source, 2), undefined);
    shouldBeArray(target, [0, 0, 1, 2, 3, 4, 0, 0]);
}

function testOverlapAndEmpty() {
    const set = warmUp(makeSet(), Uint8Array);
    const target = new Uint8Array(new ArrayBuffer(16));
    for (let i = 0; i < 16; i++)
        target[i] = i;
    set(target, target.subarray(0, 8), 4);
    shouldBeArray(target, [0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15]);
    set(target, target.subarray(8, 16), 4);
    shouldBeArray(target, [0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15, 12, 13, 14, 15]);
    set(target, target, 0);
    shouldBeArray(target, [0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15, 12, 13, 14, 15]);

    const empty = new Uint8Array(0);
    const emptyWithBuffer = new Uint8Array(new ArrayBuffer(0));
    shouldBe(set(empty, empty, 0), undefined);
    shouldBe(set(emptyWithBuffer, empty, 0), undefined);
    shouldBe(set(empty, emptyWithBuffer, 0), undefined);
    shouldBe(set(target, empty, 16), undefined);
    shouldThrow(() => set(empty, empty, 1), RangeError);
    shouldThrow(() => set(target, empty, 17), RangeError);
}

function testOversize() {
    const set = makeSet();
    const source = new Int32Array(2000);
    const target = new Int32Array(4000);
    for (let i = 0; i < source.length; i++)
        source[i] = i;
    for (let i = 0; i < testLoopCount; i++)
        set(target, source, i & 1023);
    set(target, source, 2000);
    for (let i = 0; i < source.length; i++)
        shouldBe(target[2000 + i], i);
    shouldThrow(() => set(target, source, 2001), RangeError);
}

function testReadAroundSet() {
    const set = makeSet("const before = target[offset]; target.set(source, offset); return before * 10 + target[offset];");
    const source = new Uint8Array([2]);
    const target = new Uint8Array(4);
    for (let i = 0; i < testLoopCount; i++) {
        target[i & 3] = 1;
        shouldBe(set(target, source, i & 3), 12);
    }
}

testCopy(Int8Array);
testCopy(Uint8Array);
testCopy(Uint8ClampedArray);
testCopy(Int16Array);
testCopy(Uint16Array);
testCopy(Int32Array);
testCopy(Uint32Array);
testCopy(Float16Array);
testCopy(Float32Array);
testCopy(Float64Array);
testCopy(BigInt64Array);
testCopy(BigUint64Array);
testWithoutOffset();
testExtraArguments();
testTailCall();
testInlined();
testOverlapAndEmpty();
testOversize();
testReadAroundSet();
