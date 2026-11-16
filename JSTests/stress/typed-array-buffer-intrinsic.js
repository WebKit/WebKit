function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
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

const constructors = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float16Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array];

function testViewOverBuffer() {
    function get(view) {
        return view.buffer;
    }
    noInline(get);

    for (const constructor of constructors) {
        const buffer = new ArrayBuffer(64);
        const views = [new constructor(buffer), new constructor(buffer, 8), new constructor(buffer, 16, 2), new constructor(buffer).subarray(1, 3)];
        for (let i = 0; i < testLoopCount; ++i)
            shouldBe(get(views[i & 3]), buffer);
    }
}

function testLengthConstructed() {
    function get(view) {
        return view.buffer;
    }
    noInline(get);

    for (const constructor of constructors) {
        const one = constructor === BigInt64Array || constructor === BigUint64Array ? 1n : 1;
        for (let i = 0; i < testLoopCount / 10; ++i) {
            const length = (i % 8 ? 16 : 1008) / constructor.BYTES_PER_ELEMENT;
            const view = new constructor(length);
            view[length - 1] = one;
            const buffer = get(view);
            shouldBe(buffer instanceof ArrayBuffer, true);
            shouldBe(buffer.byteLength, length * constructor.BYTES_PER_ELEMENT);
            shouldBe(get(view), buffer);
            shouldBe(new constructor(buffer)[length - 1], one);
            shouldBe(view[length - 1], one);
        }
    }
}

function testStorageAfterConversion() {
    function writeAround(view) {
        view[0] = 1;
        const buffer = view.buffer;
        view[1] = 2;
        return buffer;
    }
    noInline(writeAround);

    function readAround(view) {
        const before = view[0];
        const buffer = view.buffer;
        new Uint8Array(buffer)[0] = 42;
        return before + view[0];
    }
    noInline(readAround);

    function namedPropertiesAround(view) {
        view.first = 1;
        const buffer = view.buffer;
        view.first = 2;
        view.second = 3;
        return buffer;
    }
    noInline(namedPropertiesAround);

    for (let i = 0; i < testLoopCount; ++i) {
        const length = i % 8 ? 16 : 1008;
        const view = new Uint8Array(length);
        const buffer = writeAround(view);
        const bytes = new Uint8Array(buffer);
        shouldBe(bytes[0], 1);
        shouldBe(bytes[1], 2);
        shouldBe(view.length, length);
        shouldBe(view.byteOffset, 0);

        const second = new Uint8Array(length);
        second[0] = 7;
        shouldBe(readAround(second), 49);

        const third = new Uint8Array(length);
        shouldBe(namedPropertiesAround(third), third.buffer);
        shouldBe(third.first, 2);
        shouldBe(third.second, 3);
    }
}

function testBufferWithoutWrapper() {
    function get(view) {
        return view.buffer;
    }
    noInline(get);

    for (let i = 0; i < testLoopCount; ++i) {
        const view = new Uint8Array(16);
        const sub = view.subarray(4, 8);
        sub[0] = i;
        const buffer = i & 1 ? get(view) : get(sub);
        shouldBe(buffer.byteLength, 16);
        shouldBe(get(view), buffer);
        shouldBe(get(sub), buffer);
        shouldBe(new Uint8Array(buffer)[4], i & 255);
    }
}

function testIdentityAcrossGC() {
    function get(view) {
        return view.buffer;
    }
    noInline(get);

    const views = [];
    for (let i = 0; i < 100; ++i) {
        const view = i & 1 ? new Uint8Array(16) : new Uint8Array(new ArrayBuffer(16));
        get(view).tag = i;
        views.push(view);
    }
    for (let round = 0; round < 2; ++round) {
        gc();
        for (let i = 0; i < testLoopCount; ++i)
            shouldBe(get(views[i % 100]).tag, i % 100);
    }
}

function testResizableAndShared() {
    function get(view) {
        return view.buffer;
    }
    noInline(get);

    const resizable = new ArrayBuffer(16, { maxByteLength: 64 });
    const shared = new SharedArrayBuffer(16);
    const growable = new SharedArrayBuffer(16, { maxByteLength: 64 });
    const cases = [
        [new Uint8Array(resizable), resizable],
        [new Uint8Array(resizable, 4, 4), resizable],
        [new Int32Array(shared), shared],
        [new Uint8Array(growable), growable],
        [new Uint8Array(growable, 4, 4), growable],
    ];
    for (let i = 0; i < testLoopCount; ++i) {
        const [view, buffer] = cases[i % cases.length];
        shouldBe(get(view), buffer);
    }
    resizable.resize(2);
    growable.grow(32);
    for (let i = 0; i < testLoopCount; ++i) {
        const [view, buffer] = cases[i % cases.length];
        shouldBe(get(view), buffer);
    }
}

function testDetached() {
    function get(view) {
        return view.buffer;
    }
    noInline(get);

    const buffer = new ArrayBuffer(16);
    const view = new Uint8Array(buffer);
    const converted = new Uint8Array(16);
    const convertedBuffer = converted.buffer;
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(get(view), buffer);
        shouldBe(get(converted), convertedBuffer);
    }
    buffer.transfer();
    convertedBuffer.transfer();
    shouldBe(buffer.detached, true);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(get(view), buffer);
        shouldBe(get(converted), convertedBuffer);
    }
}

function testSubclassAndOtherRealm() {
    function get(view) {
        return view.buffer;
    }
    noInline(get);

    class Bytes extends Uint8Array { }
    const other = createGlobalObject();
    const otherBuffer = new other.ArrayBuffer(16);
    const buffer = new ArrayBuffer(16);
    const subclassed = new Bytes(buffer);
    const crossRealm = new other.Uint8Array(otherBuffer);
    const mixed = new other.Uint8Array(buffer);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(get(subclassed), buffer);
        shouldBe(get(crossRealm), otherBuffer);
        shouldBe(get(mixed), buffer);
        const fresh = get(new other.Uint8Array(4));
        shouldBe(fresh instanceof other.ArrayBuffer, true);
        shouldBe(fresh instanceof ArrayBuffer, false);
        shouldBe(get(new Bytes(4)) instanceof ArrayBuffer, true);
    }
}

function testOtherReceivers() {
    function get(object) {
        return object.buffer;
    }
    noInline(get);

    const buffer = new ArrayBuffer(16);
    const view = new Uint8Array(buffer);
    const dataView = new DataView(buffer);
    const plain = { buffer: 42 };
    const inheriting = Object.create(view);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(get(view), buffer);
        if (i > testLoopCount / 2) {
            shouldBe(get(dataView), buffer);
            shouldBe(get(plain), 42);
        }
        if (!(i % 100))
            shouldThrow(() => get(inheriting), TypeError);
    }
}

function testLiveValuesAroundAccess() {
    function get(object, a, b) {
        const x = a * 5;
        const y = b + 11;
        const z = a ^ b;
        let result;
        try {
            result = object.buffer;
        } catch {
            return -1;
        }
        return result.byteLength + x - y + z;
    }
    noInline(get);

    const buffer = new ArrayBuffer(16);
    const objects = [{ buffer }, { other: 1, buffer }, new Float64Array(buffer)];
    for (let i = 0; i < testLoopCount; ++i) {
        const a = i & 1023;
        const b = (i * 7) & 255;
        const object = i % 4 ? objects[i % 3] : new Float64Array(2);
        shouldBe(get(object, a, b), 16 + a * 5 - (b + 11) + (a ^ b));
    }
}

function testGetterCall() {
    const bufferGetter = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(Uint8Array.prototype), "buffer").get;
    function call(receiver) {
        return bufferGetter.call(receiver);
    }
    noInline(call);

    const buffer = new ArrayBuffer(16);
    const view = new Float64Array(buffer);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(call(view), buffer);
    shouldThrow(() => call(new DataView(buffer)), TypeError);
    shouldThrow(() => call({ }), TypeError);
    shouldThrow(() => call(buffer), TypeError);
    shouldThrow(() => call(undefined), TypeError);
}

function testUnusedResult() {
    function touch(view) {
        view.buffer;
        return view;
    }
    noInline(touch);

    for (let i = 0; i < testLoopCount; ++i) {
        const view = touch(new Uint8Array(8));
        view[0] = i;
        shouldBe(new Uint8Array(view.buffer)[0], i & 255);
    }
}

testViewOverBuffer();
testLengthConstructed();
testStorageAfterConversion();
testBufferWithoutWrapper();
testIdentityAcrossGC();
testResizableAndShared();
testDetached();
testSubclassAndOtherRealm();
testOtherReceivers();
testLiveValuesAroundAccess();
testGetterCall();
testUnusedResult();
