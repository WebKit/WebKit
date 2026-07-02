description(
"This tests Symbol object behaviors."
);

// Symbol constructor cannot be called with `new` style.
// To create Symbol object, call `Object(symbol)`.
shouldThrow("new Symbol", "\"TypeError: function is not a constructor (evaluating 'new Symbol')\"");
shouldThrow("new Symbol('Cappuccino')", "\"TypeError: function is not a constructor (evaluating 'new Symbol('Cappuccino')')\"");

var symbolObject = Object(Symbol.iterator);
shouldBeTrue("symbolObject instanceof Symbol");
// Since Symbol object's @@toPrimitive returns Symbol value,
// ToString(symbol) will be called.
shouldThrow("String(symbolObject)", `"TypeError: Cannot convert a symbol to a string"`);
shouldBeEqualToString("symbolObject.toString()", "Symbol(Symbol.iterator)");

var object = {};
object[symbolObject] = 42;
// ToPropertyKey(symbolObject) will call toPrimitive(symbolObject), and toPrimitive(symbolObject) will return symbol primitive value. As a result, object[symbolObject] equals to object[symbol in the symbolObject].
shouldBe("object[symbolObject]", "42");
shouldBe("object['Symbol(Symbol.iterator)']", "undefined");
shouldBe("object[Symbol.iterator]", "42");

var symbol = Symbol("Matcha");
object[symbol] = 'Cocoa';
shouldBeEqualToString("object[symbol]", "Cocoa");
shouldBeEqualToString("object[symbol.valueOf()]", "Cocoa");
shouldBeEqualToString("object[Object(symbol)]", "Cocoa");
shouldBe("object['Matcha']", "undefined");

// ToObject will be called.
shouldBe("Symbol.iterator.hello", "undefined");

successfullyParsed = true;
