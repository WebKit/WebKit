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

function makeSetWithCall() {
    return makeSet("return Uint8Array.prototype.set.call(target, source, offset);");
}

function warmUp(set, targetConstructor = Uint8Array) {
    const source = new Uint8Array([1, 2, 3, 4]);
    const target = new targetConstructor(8);
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(set(target, source, i & 3), undefined);
    shouldBe(set(target, source, 4), undefined);
    shouldBeArray(target.subarray(4), [1, 2, 3, 4]);
    return set;
}

const source = new Uint8Array([1, 2, 3, 4]);

function testOffsets() {
    {
        const set = warmUp(makeSet());
        const target = new Uint8Array(8);
        for (const offset of [5, 8, 9, -1, -5, 2 ** 31 - 1, -(2 ** 31), 2 ** 31, 2 ** 32, 2 ** 53, Infinity, -Infinity, -1.5])
            shouldThrow(() => set(target, source, offset), RangeError);
        shouldBeArray(target, [0, 0, 0, 0, 0, 0, 0, 0]);
    }

    {
        const set = warmUp(makeSet());
        for (const [offset, expected] of [[1.5, 1], [-0.5, 0], [NaN, 0], [-0, 0], [undefined, 0], [null, 0], ["2", 2], [true, 1], [{ valueOf() { return 3; } }, 3], [4, 4]]) {
            const target = new Uint8Array(8);
            shouldBe(set(target, source, offset), undefined);
            for (let j = 0; j < 8; j++)
                shouldBe(target[j], j >= expected && j < expected + 4 ? source[j - expected] : 0);
        }
        const target = new Uint8Array(8);
        shouldThrow(() => set(target, source, { valueOf() { throw new SyntaxError(); } }), SyntaxError);
        shouldThrow(() => set(target, source, Symbol()), TypeError);
        shouldThrow(() => set(target, source, 1n), TypeError);
        shouldBeArray(target, [0, 0, 0, 0, 0, 0, 0, 0]);
    }
}

function testDetached(set) {
    const target = new Uint8Array(8);
    const detached = new Uint8Array(new ArrayBuffer(64));
    $.detachArrayBuffer(detached.buffer);
    shouldThrow(() => set(target, detached, 0), TypeError);
    shouldThrow(() => set(detached, source, 0), TypeError);
    shouldThrow(() => set(detached, new Uint8Array(0), 0), TypeError);
    shouldThrow(() => set(detached, detached, 0), TypeError);
    shouldBeArray(target, [0, 0, 0, 0, 0, 0, 0, 0]);
}

function testResizable(set) {
    {
        const buffer = new ArrayBuffer(8, { maxByteLength: 16 });
        const fixedLength = new Uint8Array(buffer, 0, 8);
        const lengthTracking = new Uint8Array(buffer);
        shouldBe(set(fixedLength, source, 4), undefined);
        shouldBeArray(fixedLength, [0, 0, 0, 0, 1, 2, 3, 4]);
        shouldBe(set(lengthTracking, source, 0), undefined);
        shouldBeArray(fixedLength, [1, 2, 3, 4, 1, 2, 3, 4]);
        buffer.resize(4);
        shouldThrow(() => set(fixedLength, source, 0), TypeError);
        shouldThrow(() => set(lengthTracking, source, 1), RangeError);
        shouldBe(set(lengthTracking, source.subarray(1), 1), undefined);
        shouldBeArray(lengthTracking, [1, 2, 3, 4]);
        buffer.resize(16);
        shouldBe(set(lengthTracking, source, 12), undefined);
        shouldBeArray(lengthTracking, [1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4]);
    }

    {
        const buffer = new ArrayBuffer(8, { maxByteLength: 16 });
        const fixedLength = new Uint8Array(buffer, 0, 4);
        const lengthTracking = new Uint8Array(buffer);
        fixedLength.set(source);
        const target = new Uint8Array(8);
        shouldBe(set(target, fixedLength, 4), undefined);
        shouldBeArray(target, [0, 0, 0, 0, 1, 2, 3, 4]);
        shouldThrow(() => set(target, lengthTracking, 1), RangeError);
        buffer.resize(2);
        shouldThrow(() => set(target, fixedLength, 0), TypeError);
        shouldBe(set(target, lengthTracking, 0), undefined);
        shouldBeArray(target, [1, 2, 0, 0, 1, 2, 3, 4]);
    }

    {
        const growable = new Uint8Array(new SharedArrayBuffer(8, { maxByteLength: 16 }));
        const shared = new Uint8Array(new SharedArrayBuffer(8));
        shouldBe(set(growable, source, 4), undefined);
        shouldBe(set(shared, growable.subarray(4, 8), 2), undefined);
        shouldBeArray(shared, [0, 0, 1, 2, 3, 4, 0, 0]);
        growable.buffer.grow(16);
        shouldBe(set(growable, shared, 8), undefined);
        shouldBeArray(growable, [0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 1, 2, 3, 4, 0, 0]);
    }
}

function testOtherTargets() {
    {
        const set = warmUp(makeSet());
        shouldThrow(() => set({ set: Uint8Array.prototype.set }, source, 0), TypeError);
    }

    {
        const set = warmUp(makeSetWithCall());
        shouldThrow(() => set([], source, 0), TypeError);
        shouldThrow(() => set(new DataView(new ArrayBuffer(8)), source, 0), TypeError);
        shouldThrow(() => set(1, source, 0), TypeError);
    }

    {
        class Derived extends Uint8Array { }
        const set = warmUp(makeSet(), Derived);
        const target = new Derived(8);
        shouldThrow(() => set(target, source, 5), RangeError);
        shouldBe(set(target, new Derived([5, 6]), 6), undefined);
        shouldBeArray(target, [0, 0, 0, 0, 0, 0, 5, 6]);
        testDetached(set);
    }

    {
        const set = warmUp(makeSet());
        const other = new (createGlobalObject().Uint8Array)(8);
        shouldBe(Uint8Array.prototype.set.call(other, source, 2), undefined);
        const target = new Uint8Array(8);
        shouldBe(set(target, other, 0), undefined);
        shouldBeArray(target, [0, 0, 1, 2, 3, 4, 0, 0]);
        shouldBe(set(other, source, 4), undefined);
        shouldBeArray(other, [0, 0, 1, 2, 1, 2, 3, 4]);
    }
}

function testOtherSources() {
    {
        const set = warmUp(makeSet());
        const target = new Uint8Array(8);
        shouldBe(set(target, new Int16Array([-1, 256, 257, 2]), 1), undefined);
        shouldBeArray(target, [0, 255, 0, 1, 2, 0, 0, 0]);
        shouldBe(set(target, new Uint8ClampedArray([7, 8]), 6), undefined);
        shouldBe(set(target, new Int8Array([-2]), 5), undefined);
        shouldBe(set(target, new Float64Array([1.5, -1]), 0), undefined);
        shouldBeArray(target, [1, 255, 0, 1, 2, 254, 7, 8]);
        shouldThrow(() => set(target, new BigInt64Array(1), 0), TypeError);
        shouldThrow(() => set(target, null, 0), TypeError);
        shouldThrow(() => set(target, undefined, 0), TypeError);
    }

    {
        const set = warmUp(makeSet());
        const target = new Uint8Array(8);
        shouldBe(set(target, [1, 2, 3], 1), undefined);
        shouldBe(set(target, { length: 2, 0: 9, 1: "10" }, 5), undefined);
        shouldBe(set(target, "78", 6), undefined);
        shouldBe(set(target, 42, 0), undefined);
        shouldBeArray(target, [0, 1, 2, 3, 0, 9, 7, 8]);
        shouldThrow(() => set(target, [1, 2, 3], 6), RangeError);
        let count = 0;
        const object = {
            get length() {
                count++;
                return 1;
            },
            get 0() {
                count++;
                return 5;
            },
        };
        shouldBe(set(target, object, 7), undefined);
        shouldBe(count, 2);
        shouldBe(target[7], 5);
    }

    {
        const set = makeSet();
        const plain = [5, 6, 7, 8];
        const target = new Uint8Array(8);
        for (let i = 0; i < testLoopCount; i++) {
            set(target, i & 1 ? source : plain, 0);
            shouldBe(target[0], i & 1 ? 1 : 5);
            set(target, i & 1 ? plain : source, 4);
            shouldBe(target[7], i & 1 ? 8 : 4);
        }
    }
}

function testReplacedFunction() {
    const set = warmUp(makeSet());
    const original = Uint8Array.prototype.set;
    let called = 0;
    Uint8Array.prototype.set = function (source, offset) {
        called++;
        return original.call(this, source, offset);
    };
    const target = new Uint8Array(8);
    shouldBe(set(target, new Uint8Array([1, 2]), 6), undefined);
    shouldBe(called, 1);
    shouldBeArray(target, [0, 0, 0, 0, 0, 0, 1, 2]);
    delete Uint8Array.prototype.set;
    shouldBe(set(target, new Uint8Array([3]), 0), undefined);
    shouldBe(called, 1);
    shouldBe(target[0], 3);
}

testOffsets();
testDetached(warmUp(makeSet()));
testDetached(warmUp(makeSetWithCall()));
testResizable(warmUp(makeSet()));
testResizable(warmUp(makeSetWithCall()));
testOtherTargets();
testOtherSources();
testReplacedFunction();
