function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var otherRealm = createGlobalObject();
shouldBe(otherRealm.Number.prototype.toLocaleString.call(new Number(42)), `42`)

var numberObject = new Number(42);
numberObject.__proto__ = null;
shouldBe(Number.prototype.toLocaleString.call(numberObject), `42`)
