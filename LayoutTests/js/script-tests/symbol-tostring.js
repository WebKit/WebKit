description(
"This tests that ToString(Symbol) behaviors."
);

shouldBeEqualToString("String(Symbol.iterator)", "Symbol(Symbol.iterator)");
shouldThrow("String(Symbol.prototype)", `"TypeError: Symbol.prototype.valueOf requires that |this| be a symbol or a symbol object"`);
shouldBeEqualToString("Symbol.prototype.toString.call(Symbol.iterator)", "Symbol(Symbol.iterator)");
shouldBeEqualToString("Symbol.prototype.toString.call(Symbol('hello'))", "Symbol(hello)");
shouldBeEqualToString("Symbol.prototype.toString.call(Symbol())", "Symbol()");
shouldBeEqualToString("Object.prototype.toString.call(Symbol())", "[object Symbol]");
shouldBeEqualToString("Object.prototype.toString.call(Symbol.prototype)", "[object Symbol]");
shouldBeEqualToString("Object.prototype.toString.call(Symbol)", "[object Function]");

successfullyParsed = true;
