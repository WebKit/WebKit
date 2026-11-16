function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldThrow(fn, errorMessage)
{
    let thrown = false;
    try {
        fn();
    } catch (e) {
        thrown = true;
        if (e.message !== errorMessage)
            throw new Error(`Expected error message "${errorMessage}" but got "${e.message}"`);
    }
    if (!thrown)
        throw new Error('Expected to throw but did not');
}

// A transferred buffer hands its contents to the clone and detaches.
{
    let buffer = new ArrayBuffer(8);
    new Uint8Array(buffer)[3] = 42;
    let clone = structuredClone({ buffer }, { transfer: [buffer] });
    shouldBe(buffer.byteLength, 0);
    shouldBe(clone.buffer.byteLength, 8);
    shouldBe(new Uint8Array(clone.buffer)[3], 42);
}

// Views over a transferred buffer come back over the transferred contents.
{
    let buffer = new ArrayBuffer(4);
    let view = new Uint8Array(buffer);
    view[1] = 5;
    let clone = structuredClone({ view }, { transfer: [buffer] });
    shouldBe(clone.view.length, 4);
    shouldBe(clone.view[1], 5);
    shouldBe(view.length, 0);
    shouldBe(buffer.byteLength, 0);
}

// A resizable buffer keeps its bounds across a transfer.
{
    let buffer = new ArrayBuffer(4, { maxByteLength: 8 });
    let clone = structuredClone({ buffer }, { transfer: [buffer] });
    shouldBe(clone.buffer.byteLength, 4);
    shouldBe(clone.buffer.maxByteLength, 8);
    shouldBe(buffer.byteLength, 0);
}

// Several buffers at once.
{
    let first = new ArrayBuffer(2);
    let second = new ArrayBuffer(3);
    let clone = structuredClone([first, second], { transfer: [first, second] });
    shouldBe(clone[0].byteLength, 2);
    shouldBe(clone[1].byteLength, 3);
    shouldBe(first.byteLength, 0);
    shouldBe(second.byteLength, 0);
}

// Listing a buffer twice is a duplicate transferable and is rejected before anything is detached.
{
    let buffer = new ArrayBuffer(4);
    shouldThrow(() => structuredClone({ buffer }, { transfer: [buffer, buffer] }), 'Duplicate transferable for structured clone');
    shouldBe(buffer.byteLength, 4);
}

// A buffer the value does not reference is still detached.
{
    let buffer = new ArrayBuffer(4);
    let clone = structuredClone({ a: 1 }, { transfer: [buffer] });
    shouldBe(clone.a, 1);
    shouldBe(buffer.byteLength, 0);
}

// transfer takes any iterable, not just an array.
{
    let buffer = new ArrayBuffer(4);
    let clone = structuredClone({ buffer }, { transfer: new Set([buffer]) });
    shouldBe(clone.buffer.byteLength, 4);
    shouldBe(buffer.byteLength, 0);

    buffer = new ArrayBuffer(4);
    let generator = function* () { yield buffer; };
    clone = structuredClone({ buffer }, { transfer: generator() });
    shouldBe(clone.buffer.byteLength, 4);
    shouldBe(buffer.byteLength, 0);
}

// An absent, empty, or nullish transfer list clones normally.
{
    shouldBe(structuredClone({ a: 1 }, { transfer: [] }).a, 1);
    shouldBe(structuredClone({ a: 1 }, { }).a, 1);
    shouldBe(structuredClone({ a: 1 }, undefined).a, 1);
    shouldBe(structuredClone({ a: 1 }, null).a, 1);
}

// Transferring happens only after the value serializes, so a failed clone leaves the buffer alone.
{
    let buffer = new ArrayBuffer(4);
    shouldThrow(() => structuredClone({ get x() { throw new Error('from the getter'); } }, { transfer: [buffer] }), 'from the getter');
    shouldBe(buffer.byteLength, 4);

    buffer = new ArrayBuffer(4);
    shouldThrow(() => structuredClone({ fn: function () { } }, { transfer: [buffer] }), 'Value could not be cloned.');
    shouldBe(buffer.byteLength, 4);
}

// A buffer detached by a getter while the value serializes is caught before it is transferred.
{
    let buffer = new ArrayBuffer(4);
    let value = {
        get detacher() {
            structuredClone({ }, { transfer: [buffer] });
            return 1;
        }
    };
    shouldThrow(() => structuredClone(value, { transfer: [buffer] }), 'Value could not be cloned.');
}

// Rejected transfer lists.
{
    let detached = new ArrayBuffer(4);
    structuredClone({ }, { transfer: [detached] });
    shouldThrow(() => structuredClone({ }, { transfer: [detached] }), 'Value could not be cloned.');

    let shared = new SharedArrayBuffer(4);
    shouldThrow(() => structuredClone({ shared }, { transfer: [shared] }), 'Value could not be cloned.');

    shouldThrow(() => structuredClone(1, { transfer: [new Uint8Array(4)] }), 'structuredClone transfer list can only contain ArrayBuffers');
    shouldThrow(() => structuredClone(1, { transfer: [{ }] }), 'structuredClone transfer list can only contain ArrayBuffers');
    shouldThrow(() => structuredClone(1, { transfer: 5 }), 'Type error');
    shouldThrow(() => structuredClone(1, 5), 'structuredClone options must be an object.');
}

// Exceptions from reading the options object propagate.
{
    shouldThrow(() => structuredClone(1, { get transfer() { throw new Error('from the options'); } }), 'from the options');
    shouldThrow(() => structuredClone(1, { transfer: { [Symbol.iterator]() { throw new Error('from the iterator'); } } }), 'from the iterator');
}
