function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage)
{
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

function toString(value)
{
    return `${value}`;
}
noInline(toString);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(toString(i), i + "");
    shouldBe(toString(null), "null");
    shouldBe(toString(undefined), "undefined");
    shouldBe(toString(10.5), "10.5");
    shouldBe(toString(-10.5), "-10.5");
    shouldBe(toString(true), "true");
    shouldBe(toString(false), "false");
    shouldBe(toString(0 / 0), "NaN");
}

shouldBe(toString("HELLO"), "HELLO");
shouldThrow(() => {
    toString(Symbol("Cocoa"));
}, `TypeError: Cannot convert a symbol to a string`);
