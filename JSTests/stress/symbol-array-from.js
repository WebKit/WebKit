function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var called = false;
Object.defineProperty(Symbol.prototype, Symbol.iterator, {
    get() {
        "use strict";
        shouldBe(typeof this, "symbol");
        called = true;
        return null;
    }
});

var symbol = Symbol("Cocoa");
{
    called =false;
    Array.from(symbol);
    shouldBe(called, true);
}
{
    called =false;
    Uint8Array.from(symbol);
    shouldBe(called, true);
}
