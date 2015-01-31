description(
"This tests that Symbol.prototype object is ordinary object (not Symbol wrapper object)."
);

shouldThrow("Symbol.prototype.toString.call(Symbol.prototype)", "'TypeError: Type error'");
shouldBeEqualToString("Symbol.prototype.toString.call(Symbol.iterator)", "Symbol(Symbol.iterator)");

successfullyParsed = true;
