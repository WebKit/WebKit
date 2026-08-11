function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
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

var array = new Uint8Array(42);
var count = 0;

shouldThrow(() => {
    for (let v of array) {
        ++count;
        $.detachArrayBuffer(array.buffer);
    }
}, `TypeError: Underlying ArrayBuffer has been detached from the view or out-of-bounds`);

shouldBe(count, 1);
