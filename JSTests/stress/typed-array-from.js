function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

{
    let a0 = new Uint32Array([0xffffffff, 0xfffffffe, 0xfffffff0, 0xfffff0f0]);
    let a1 = Uint8Array.from(a0);
    shouldBeArray(a1, [0xff, 0xfe, 0xf0, 0xf0]);

    let a2 = Uint32Array.from(a0);
    shouldBeArray(a2, a0);
}
{
    class TestArray extends Uint8Array {
        constructor(size) {
            super(size);
        }
    }

    let a0 = new Uint32Array([0xffffffff, 0xfffffffe, 0xfffffff0, 0xfffff0f0]);
    let a1 = TestArray.from(a0);
    shouldBeArray(a1, [0xff, 0xfe, 0xf0, 0xf0]);

    let a2 = Uint32Array.from(a0);
    shouldBeArray(a2, a0);
}
{
    let a0 = new Uint32Array([0xffffffff, 0xfffffffe, 0xfffffff0, 0xfffff0f0]);
    class TestArray extends Uint8Array {
        constructor(size) {
            super(size);
            transferArrayBuffer(a0.buffer);
        }
    }

    let a1 = TestArray.from(a0);
    shouldBeArray(a1, [0xff, 0xfe, 0xf0, 0xf0]); // This should pass. When TestArray is created, we should have already collected the data from a0.

    shouldThrow(() => {
        Uint32Array.from(a0);
    }, `TypeError: Underlying ArrayBuffer has been detached from the view or out-of-bounds`);
}

Uint8Array.prototype.__proto__[Symbol.iterator] = function *() {
    for (var i = 0; i < this.length; ++i)
        yield 42;
};

{
    let a0 = new Uint32Array([0xffffffff, 0xfffffffe, 0xfffffff0, 0xfffff0f0]);
    let a1 = Uint8Array.from(a0);
    shouldBeArray(a1, [42, 42, 42, 42]);
}
