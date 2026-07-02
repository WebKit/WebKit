function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var numberCount = 0;
var stringCount = 0;
var booleanCount = 0;
var symbolCount = 0;
var bigIntCount = 0;
var spy;

spy = new Proxy({}, { set: function() { numberCount += 1; return true; } });
Object.setPrototypeOf(Number.prototype, spy);
0..property = null;
shouldBe(numberCount, 1);

spy = new Proxy({}, { set: function() { stringCount += 1; return true; } });
Object.setPrototypeOf(String.prototype, spy);
"".property = null;
shouldBe(stringCount, 1);

spy = new Proxy({}, { set: function() { booleanCount += 1; return true; } });
Object.setPrototypeOf(Boolean.prototype, spy);
true.property = null;
shouldBe(booleanCount, 1);

spy = new Proxy({}, { set: function() { symbolCount += 1; return true; } });
Object.setPrototypeOf(Symbol.prototype, spy);
Symbol().property = null;
shouldBe(symbolCount, 1);


spy = new Proxy({}, { set: function() { bigIntCount += 1; return true; } });
Object.setPrototypeOf(BigInt.prototype, spy);
(1n).property = null;
shouldBe(bigIntCount, 1);
