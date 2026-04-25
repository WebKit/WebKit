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

bar = 0;
function foo(code) {
    eval(code);
    return (function () {
        return bar;
    }());
}
shouldBe(foo(`42`), 0);

$.evalScript(`let bar = 42`);
shouldBe(foo(`42`), 42);

shouldBe(foo(`var bar = 1`), 1);
shouldBe(foo(`42`), 42);
