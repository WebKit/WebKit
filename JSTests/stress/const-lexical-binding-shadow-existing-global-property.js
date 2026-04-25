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
noInline(shouldThrow);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}
noInline(shouldBe);

function foo() {
    bar = 4;
}
foo();
shouldBe(bar, 4);
$.evalScript('const bar = 3;');
shouldBe(bar, 3);
shouldThrow(() => {
    foo();
}, `TypeError: Attempted to assign to readonly property.`);
shouldBe(bar, 3);
