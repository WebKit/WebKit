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

function createIterator() {
    return {
        next() { return { done: true } },
        return() { this.closed = true }
    };
}

{
    let iter = createIterator();
    shouldBe(iter.closed, undefined);
    shouldThrow(() => {
        Iterator.prototype.drop.call(iter, NaN);
    }, `RangeError: Iterator.prototype.drop argument must not be NaN.`);
    shouldBe(iter.closed, true);
}
{
    let iter = createIterator();
    shouldBe(iter.closed, undefined);
    shouldThrow(() => {
        Iterator.prototype.take.call(iter, NaN);
    }, `RangeError: Iterator.prototype.take argument must not be NaN.`);
    shouldBe(iter.closed, true);
}
