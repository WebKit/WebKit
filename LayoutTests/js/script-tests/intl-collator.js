description("This test checks the behavior of Intl.Collator as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 10.1 The Intl.Collator Constructor

// The Intl.Collator constructor is a standard built-in property of the Intl object.
shouldBeType("Intl.Collator", "Function");

// 10.1.2 Intl.Collator([ locales [, options]])
shouldBeType("Intl.Collator()", "Intl.Collator");
shouldBeType("Intl.Collator.call({})", "Intl.Collator");
shouldBeType("new Intl.Collator()", "Intl.Collator");

// Subclassable
class DerivedCollator extends Intl.Collator {}
shouldBeType("new DerivedCollator", "DerivedCollator");
shouldBeType("new DerivedCollator", "Intl.Collator");
shouldBe("new DerivedCollator().compare('a', 'b')", "-1");
shouldBe("Object.getPrototypeOf(new DerivedCollator)", "DerivedCollator.prototype");
shouldBe("Object.getPrototypeOf(Object.getPrototypeOf(new DerivedCollator))", "Intl.Collator.prototype");

// 10.2 Properties of the Intl.Collator Constructor

// length property (whose value is 0)
shouldBe("Intl.Collator.length", "0");

// 10.2.1 Intl.Collator.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').enumerable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').configurable");

// 10.2.2 Intl.Collator.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe("Intl.Collator.supportedLocalesOf.length", "1");

// Returns SupportedLocales
shouldBeType("Intl.Collator.supportedLocalesOf()", "Array");

// 10.3 Properties of the Intl.Collator Prototype Object

// The value of Intl.Collator.prototype.constructor is %Collator%.
shouldBe("Intl.Collator.prototype.constructor", "Intl.Collator");

// 10.3.3 Intl.Collator.prototype.compare

// This named accessor property returns a function that compares two strings according to the sort order of this Collator object.
shouldBeType("Intl.Collator.prototype.compare", "Function");

// The value of the [[Get]] attribute is a function
shouldBeType("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').get", "Function");

// The value of the [[Set]] attribute is undefined.
shouldBe("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').set", "undefined");

// Match Firefox where unspecifed.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').configurable");

// The value of F’s length property is 2.
shouldBe("Intl.Collator.prototype.compare.length", "2");

// Throws on non-Collator this.
shouldThrow("Object.defineProperty({}, 'compare', Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare')).compare", "'TypeError: Intl.Collator.prototype.compare called on value that\\'s not an object initialized as a Collator'");

// The compare function is unique per instance.
shouldBeTrue("Intl.Collator.prototype.compare !== Intl.Collator().compare");
shouldBeTrue("new Intl.Collator().compare !== new Intl.Collator().compare");

// 10.3.4 Collator Compare Functions

// 1. Let collator be the this value.
// 2. Assert: Type(collator) is Object and collator has an [[initializedCollator]] internal slot whose value is true.
// This should not be reachable, since compare is bound to an initialized collator.

// 3. If x is not provided, let x be undefined.
// 4. If y is not provided, let y be undefined.
// 5. Let X be ToString(x).
// 6. ReturnIfAbrupt(X).
var badCalls = 0;
shouldThrow("Intl.Collator.prototype.compare({ toString() { throw Error('6') } }, { toString() { ++badCalls; return ''; } })", "'Error: 6'");
shouldBe("badCalls", "0");

// 7. Let Y be ToString(y).
// 8. ReturnIfAbrupt(Y).
shouldThrow("Intl.Collator.prototype.compare('a', { toString() { throw Error('8') } })", "'Error: 8'");

// Compare is bound, so calling with alternate "this" has no effect.
shouldBe("Intl.Collator.prototype.compare.call(null, 'a', 'b')", "-1");
shouldBe("Intl.Collator.prototype.compare.call(Intl.Collator('en', { sensitivity:'base' }), 'A', 'a')", "-1");
shouldBe("Intl.Collator.prototype.compare.call(5, 'a', 'b')", "-1");
shouldBe("new Intl.Collator().compare.call(null, 'a', 'b')", "-1");
shouldBe("new Intl.Collator().compare.call(Intl.Collator('en', { sensitivity:'base' }), 'A', 'a')", "-1");
shouldBe("new Intl.Collator().compare.call(5, 'a', 'b')", "-1");

// 10.3.5 Intl.Collator.prototype.resolvedOptions ()

shouldBe("Intl.Collator.prototype.resolvedOptions.length", "0");

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBeType("Intl.Collator.prototype.resolvedOptions()", "Object");

// Returns a new object each time.
shouldBeFalse("Intl.Collator.prototype.resolvedOptions() === Intl.Collator.prototype.resolvedOptions()");

// Throws on non-Collator this.
shouldThrow("Intl.Collator.prototype.resolvedOptions.call(5)", "'TypeError: Intl.Collator.prototype.resolvedOptions called on value that\\'s not an object initialized as a Collator'");

