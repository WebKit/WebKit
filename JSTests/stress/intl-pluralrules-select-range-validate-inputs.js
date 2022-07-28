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

if (Intl.PluralRules.prototype.selectRange) {
    let pl = new Intl.PluralRules('en-US');
    shouldThrow(() => {
        pl.selectRange();
    }, `TypeError: start or end is undefined`);
    shouldThrow(() => {
        pl.selectRange(0, undefined);
    }, `TypeError: start or end is undefined`);
    shouldThrow(() => {
        pl.selectRange(undefined, 0);
    }, `TypeError: start or end is undefined`);
    shouldThrow(() => {
        pl.selectRange(undefined, undefined);
    }, `TypeError: start or end is undefined`);
    shouldThrow(() => {
        pl.selectRange(NaN, 0);
    }, `RangeError: Passed numbers are out of range`);
    shouldThrow(() => {
        pl.selectRange(0, NaN);
    }, `RangeError: Passed numbers are out of range`);
    shouldThrow(() => {
        pl.selectRange(NaN, NaN);
    }, `RangeError: Passed numbers are out of range`);
    shouldBe(pl.selectRange(0, -0), `other`);
    shouldBe(pl.selectRange(20, -20), `other`);
}
