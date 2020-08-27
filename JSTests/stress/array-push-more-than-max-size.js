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

let a = Array(2**32-1);
shouldThrow(() => {
    a.push(0, 0);
}, `RangeError: Invalid array length`);
shouldBe(a.length, 2**32 - 1);
shouldBe(a[2**32], 0);
shouldBe(a[2**32 - 1], 0);
shouldBe(a[2**32 - 2], undefined);
