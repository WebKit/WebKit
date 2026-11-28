function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " !== " + expected);
}

function shouldThrow(fn, errorType) {
    let thrown = null;
    try {
        fn();
    } catch (error) {
        thrown = error;
    }
    if (!thrown)
        throw new Error("expected " + errorType.name);
    if (!(thrown instanceof errorType))
        throw new Error("expected " + errorType.name + ", got " + thrown);
}

function load(array, index) {
    return array[index];
}
noInline(load);

function store(array, index, value) {
    array[index] = value;
}
noInline(store);

function copy(destination, source) {
    for (let i = 0; i < source.length; ++i)
        destination[i] = source[i];
}
noInline(copy);

function atomicLoad(array, index) {
    return Atomics.load(array, index);
}
noInline(atomicLoad);

function atomicStore(array, index, value) {
    return Atomics.store(array, index, value);
}
noInline(atomicStore);

function atomicAdd(array, index, value) {
    return Atomics.add(array, index, value);
}
noInline(atomicAdd);

function atomicSub(array, index, value) {
    return Atomics.sub(array, index, value);
}
noInline(atomicSub);

function atomicAnd(array, index, value) {
    return Atomics.and(array, index, value);
}
noInline(atomicAnd);

function atomicOr(array, index, value) {
    return Atomics.or(array, index, value);
}
noInline(atomicOr);

function atomicXor(array, index, value) {
    return Atomics.xor(array, index, value);
}
noInline(atomicXor);

function atomicExchange(array, index, value) {
    return Atomics.exchange(array, index, value);
}
noInline(atomicExchange);

function atomicCompareExchange(array, index, expected, replacement) {
    return Atomics.compareExchange(array, index, expected, replacement);
}
noInline(atomicCompareExchange);

const iterations = 10000;

function testElementAccess(Ctor, signed) {
    let array = new Ctor(4);
    array[0] = 0n;
    array[1] = 1n;
    array[2] = -1n;
    array[3] = 42n;

    for (let i = 0; i < iterations; ++i) {
        shouldBe(load(array, 0), 0n);
        shouldBe(load(array, 1), 1n);
        shouldBe(load(array, 2), signed ? -1n : (2n ** 64n - 1n));
        shouldBe(load(array, 3), 42n);
    }

    shouldBe(load(array, 4), undefined);
    shouldBe(load(array, -1), undefined);

    for (let i = 0; i < iterations; ++i)
        store(array, 1, 7n);
    shouldBe(array[1], 7n);

    store(array, 8, 99n);
    shouldBe(array[1], 7n);
    shouldBe(load(array, 8), undefined);

    store(array, 0, 2n ** 64n + 5n);
    shouldBe(load(array, 0), 5n);

    store(array, 0, 2n ** 63n);
    shouldBe(load(array, 0), signed ? -(2n ** 63n) : 2n ** 63n);

    store(array, 0, -1n);
    shouldBe(load(array, 0), signed ? -1n : (2n ** 64n - 1n));

    let source = new Ctor(4);
    source[0] = 11n;
    source[1] = 22n;
    source[2] = 33n;
    source[3] = 44n;
    let destination = new Ctor(4);
    for (let i = 0; i < iterations; ++i)
        copy(destination, source);
    shouldBe(destination[0], 11n);
    shouldBe(destination[1], 22n);
    shouldBe(destination[2], 33n);
    shouldBe(destination[3], 44n);

    let mixed = new Ctor(1);
    mixed[0] = 1n;
    for (let i = 0; i < iterations; ++i)
        store(mixed, 0, 1n);
    shouldThrow(() => store(mixed, 0, 1), TypeError);
    shouldBe(mixed[0], 1n);
}

function testAtomics(Ctor) {
    let array = new Ctor(new SharedArrayBuffer(32));
    array[0] = 10n;

    for (let i = 0; i < iterations; ++i)
        shouldBe(atomicLoad(array, 0), 10n);

    let wide = 2n ** 80n + 9n;
    for (let i = 0; i < iterations; ++i)
        shouldBe(atomicStore(array, 1, wide), wide);
    shouldBe(atomicLoad(array, 1), 9n);
    shouldBe(array[1], 9n);

    shouldBe(atomicAdd(array, 0, 3n), 10n);
    shouldBe(atomicLoad(array, 0), 13n);
    shouldBe(atomicAdd(array, 0, 2n ** 64n), 13n);
    shouldBe(atomicLoad(array, 0), 13n);

    shouldBe(atomicSub(array, 0, 4n), 13n);
    shouldBe(atomicLoad(array, 0), 9n);

    shouldBe(atomicAnd(array, 0, 7n), 9n);
    shouldBe(atomicLoad(array, 0), 1n);

    shouldBe(atomicOr(array, 0, 8n), 1n);
    shouldBe(atomicLoad(array, 0), 9n);

    shouldBe(atomicXor(array, 0, 1n), 9n);
    shouldBe(atomicLoad(array, 0), 8n);

    shouldBe(atomicExchange(array, 0, 20n), 8n);
    shouldBe(atomicLoad(array, 0), 20n);

    shouldBe(atomicCompareExchange(array, 0, 1n, 30n), 20n);
    shouldBe(atomicLoad(array, 0), 20n);
    shouldBe(atomicCompareExchange(array, 0, 20n, 30n), 20n);
    shouldBe(atomicLoad(array, 0), 30n);

    let throwing = new Ctor(1);
    throwing[0] = 1n;
    for (let i = 0; i < iterations; ++i)
        atomicAdd(throwing, 0, 0n);
    shouldThrow(() => atomicAdd(throwing, 0, 1), TypeError);
    shouldBe(throwing[0], 1n);
}

function testResizable() {
    let buffer = new ArrayBuffer(16, { maxByteLength: 64 });
    let array = new BigInt64Array(buffer);
    array[0] = 7n;
    for (let i = 0; i < iterations; ++i)
        shouldBe(load(array, 0), 7n);
    buffer.resize(8);
    shouldBe(load(array, 1), undefined);
    shouldBe(load(array, 0), 7n);
    buffer.resize(32);
    store(array, 3, 5n);
    shouldBe(load(array, 3), 5n);
}

function testDetach() {
    let array = new BigInt64Array(2);
    array[0] = 4n;
    for (let i = 0; i < iterations; ++i)
        shouldBe(load(array, 0), 4n);
    $.detachArrayBuffer(array.buffer);
    shouldBe(load(array, 0), undefined);
}

testElementAccess(BigInt64Array, true);
testElementAccess(BigUint64Array, false);
testAtomics(BigInt64Array);
testAtomics(BigUint64Array);
testResizable();
testDetach();

function intAtomicAdd(array, index, value) {
    return Atomics.add(array, index, value);
}
noInline(intAtomicAdd);

let int32 = new Int32Array(new SharedArrayBuffer(16));
for (let i = 0; i < iterations; ++i)
    intAtomicAdd(int32, 0, 1);
shouldBe(int32[0], iterations);
