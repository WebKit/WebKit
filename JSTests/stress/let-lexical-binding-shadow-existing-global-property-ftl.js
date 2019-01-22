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
function get() {
    return bar;
}
for (var i = 0; i < 1e6; ++i)
    foo();
for (var i = 0; i < 1e6; ++i)
    shouldBe(get(), 4);

shouldBe(bar, 4);
$.evalScript('let bar = 3;');
shouldBe(bar, 3);
shouldBe(get(), 3);

for (var i = 0; i < 1e6; ++i)
    shouldBe(get(), 3);

foo();
shouldBe(bar, 4);
shouldBe(get(), 4);

for (var i = 0; i < 1e6; ++i)
    shouldBe(get(), 4);
