function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
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

var detachedMessage = 'TypeError: Underlying ArrayBuffer has been detached from the view or out-of-bounds';

function sum(iterable) {
    var result = 0;
    for (var value of iterable)
        result += value;
    return result;
}
noInline(sum);

function step(iterator) {
    return iterator.next();
}
noInline(step);

for (var i = 0; i < testLoopCount; ++i) {
    var array = new Int32Array([1, 2, 3]);
    shouldBe(sum(array), 6);

    if (i % 10)
        continue;

    // Detaching mid-iteration throws on the following next().
    var iterator = array.values();
    shouldBe(step(iterator).value, 1);
    $.detachArrayBuffer(array.buffer);
    shouldThrow(() => step(iterator), detachedMessage);

    // An exhausted iterator does not look at the buffer again.
    var exhausted = new Int32Array([1]);
    var exhaustedIterator = exhausted.values();
    step(exhaustedIterator);
    shouldBe(step(exhaustedIterator).done, true);
    $.detachArrayBuffer(exhausted.buffer);
    shouldBe(step(exhaustedIterator).done, true);

    shouldThrow(() => {
        var detached = new Float64Array(4);
        for (var value of detached)
            $.detachArrayBuffer(detached.buffer);
    }, detachedMessage);

    // Resizable buffers: the length is re-read, and an out-of-bounds view throws.
    var buffer = new ArrayBuffer(4, { maxByteLength: 8 });
    var tracking = new Uint8Array(buffer);
    var tracked = [];
    for (var value of tracking) {
        tracked.push(value);
        if (buffer.byteLength === 4)
            buffer.resize(6);
    }
    shouldBe(tracked.length, 6);

    var fixed = new Uint8Array(buffer, 2, 4);
    var fixedIterator = fixed.values();
    step(fixedIterator);
    buffer.resize(3);
    shouldThrow(() => step(fixedIterator), detachedMessage);
}

// Empty typed arrays are done right away, not detached.
var empty = new Uint16Array(0);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(sum(empty), 0);
