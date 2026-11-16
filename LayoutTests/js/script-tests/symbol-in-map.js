description(
"This tests Symbols work in ES6 Map."
);

var symbol = Symbol("Cocoa");
var symbol2 = Symbol("Matcha");
var map = new Map();

map.set(symbol, "Cappuccino");
shouldBe("map.size", "1");
shouldBeEqualToString("map.get(symbol)", "Cappuccino");
shouldBeFalse("map.has(Symbol('Cocoa'))");
shouldBe("map.get(Symbol('Cocoa'))", "undefined");
shouldBeFalse("map.has('Cocoa')");
shouldBe("map.get('Cocoa')", "undefined");
shouldBeFalse("map.has(symbol2)");
shouldBe("map.get(symbol2)", "undefined");

map.set(symbol2, "Kilimanjaro");
shouldBe("map.size", "2");
shouldBeEqualToString("map.get(symbol)", "Cappuccino");
shouldBeEqualToString("map.get(symbol2)", "Kilimanjaro");
shouldBeFalse("map.has(Symbol('Matcha'))");
shouldBe("map.get(Symbol('Matcha'))", "undefined");
shouldBeFalse("map.has('Matcha')");
shouldBe("map.get('Matcha')", "undefined");

map.delete(symbol2);
shouldBeFalse("map.has(symbol2)");
shouldBe("map.get(symbol2)", "undefined");
shouldBeTrue("map.has(symbol)");
shouldBeEqualToString("map.get(symbol)", "Cappuccino");

shouldBe("map.size", "1");
var key, value;
map.forEach(function (v, k) {
    key = k;
    value = v;
});
shouldBe("key", "symbol");
shouldBeEqualToString("value", "Cappuccino");

successfullyParsed = true;
