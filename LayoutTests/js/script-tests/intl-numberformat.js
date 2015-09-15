description("This test checks the behavior of Intl.NumberFormat as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 11.1 The Intl.NumberFormat Constructor

// The Intl.NumberFormat constructor is a standard built-in property of the Intl object.
shouldBeType("Intl.NumberFormat", "Function");

// 11.1.2 Intl.NumberFormat([ locales [, options]])
shouldBeType("Intl.NumberFormat()", "Intl.NumberFormat");
shouldBeType("Intl.NumberFormat.call({})", "Intl.NumberFormat");
shouldBeType("new Intl.NumberFormat()", "Intl.NumberFormat");

// Subclassable
var classPrefix = "class DerivedNumberFormat extends Intl.NumberFormat {};";
shouldBeTrue(classPrefix + "(new DerivedNumberFormat) instanceof DerivedNumberFormat");
shouldBeTrue(classPrefix + "(new DerivedNumberFormat) instanceof Intl.NumberFormat");
shouldBeTrue(classPrefix + "new DerivedNumberFormat().format(1) === '1'");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(new DerivedNumberFormat) === DerivedNumberFormat.prototype");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(Object.getPrototypeOf(new DerivedNumberFormat)) === Intl.NumberFormat.prototype");

// 11.2 Properties of the Intl.NumberFormat Constructor

// length property (whose value is 0)
shouldBe("Intl.NumberFormat.length", "0");

// 11.2.1 Intl.NumberFormat.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').enumerable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').configurable");

// 11.2.2 Intl.NumberFormat.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe("Intl.NumberFormat.supportedLocalesOf.length", "1");

// Returns SupportedLocales
shouldBeType("Intl.NumberFormat.supportedLocalesOf()", "Array");
// Doesn't care about `this`.
shouldBe("Intl.NumberFormat.supportedLocalesOf.call(null, 'en')", "[ 'en' ]");
shouldBe("Intl.NumberFormat.supportedLocalesOf.call({}, 'en')", "[ 'en' ]");
shouldBe("Intl.NumberFormat.supportedLocalesOf.call(1, 'en')", "[ 'en' ]");
// Ignores non-object, non-string list.
shouldBe("Intl.NumberFormat.supportedLocalesOf(9)", "[]");
// Makes an array of tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('en')", "[ 'en' ]");
// Handles array-like objects with holes.
shouldBe("Intl.NumberFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })", "[ 'es', 'en', 'de' ]");
// Deduplicates tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])", "[ 'en', 'pt', 'es' ]");
// Canonicalizes tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('En-laTn-us-variant2-variant1-1abc-U-ko-tRue-A-aa-aaa-x-RESERVED')", "[ 'en-Latn-US-variant2-variant1-1abc-a-aa-aaa-u-ko-true-x-reserved' ]");
// Replaces outdated tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('no-bok')", "[ 'nb' ]");
// Doesn't throw, but ignores private tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('x-some-thing')", "[]");
// Throws on problems with length, get, or toString.
shouldThrow("Intl.NumberFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("Intl.NumberFormat.supportedLocalesOf([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('')", "'RangeError: invalid language tag: '");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('a')", "'RangeError: invalid language tag: a'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en--US')", "'RangeError: invalid language tag: en--US'");

// 11.3 Properties of the Intl.NumberFormat Prototype Object

// The value of Intl.NumberFormat.prototype.constructor is %NumberFormat%.
shouldBe("Intl.NumberFormat.prototype.constructor", "Intl.NumberFormat");

// 11.3.3 Intl.NumberFormat.prototype.format

// This named accessor property returns a function that formats a number according to the effective locale and the formatting options of this NumberFormat object.
shouldBeType("Intl.NumberFormat.prototype.format", "Function");

// The value of the [[Get]] attribute is a function
shouldBeType("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').get", "Function");

// The value of the [[Set]] attribute is undefined.
shouldBe("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').set", "undefined");

// Match Firefox where unspecifed.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').configurable");

// The value of F’s length property is 1.
shouldBe("Intl.NumberFormat.prototype.format.length", "1");

// Throws on non-NumberFormat this.
shouldThrow("Object.defineProperty({}, 'format', Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format')).format", "'TypeError: Intl.NumberFormat.prototype.format called on value that\\'s not an object initialized as a NumberFormat'");

// The format function is unique per instance.
shouldBeTrue("Intl.NumberFormat.prototype.format !== Intl.NumberFormat().format");
shouldBeTrue("new Intl.NumberFormat().format !== new Intl.NumberFormat().format");

// 11.3.4 Format Number Functions

// 1. Let nf be the this value.
// 2. Assert: Type(nf) is Object and nf has an [[initializedNumberFormat]] internal slot whose value is true.
// This should not be reachable, since format is bound to an initialized numberformat.

// 3. If value is not provided, let value be undefined.
// 4. Let x be ToNumber(value).
// 5. ReturnIfAbrupt(x).
shouldThrow("Intl.NumberFormat.prototype.format({ valueOf() { throw Error('5') } })", "'Error: 5'");

// Format is bound, so calling with alternate "this" has no effect.
shouldBe("Intl.NumberFormat.prototype.format.call(null, 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("Intl.NumberFormat.prototype.format.call(Intl.DateTimeFormat('ar'), 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("Intl.NumberFormat.prototype.format.call(5, 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("new Intl.NumberFormat().format.call(null, 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("new Intl.NumberFormat().format.call(Intl.DateTimeFormat('ar'), 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("new Intl.NumberFormat().format.call(5, 1.2)", "Intl.NumberFormat().format(1.2)");

// 11.3.5 Intl.NumberFormat.prototype.resolvedOptions ()

shouldBe("Intl.NumberFormat.prototype.resolvedOptions.length", "0");

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBeType("Intl.NumberFormat.prototype.resolvedOptions()", "Object");

// Returns a new object each time.
shouldBeFalse("Intl.NumberFormat.prototype.resolvedOptions() === Intl.NumberFormat.prototype.resolvedOptions()");

// Throws on non-NumberFormat this.
shouldThrow("Intl.NumberFormat.prototype.resolvedOptions.call(5)", "'TypeError: Intl.NumberFormat.prototype.resolvedOptions called on value that\\'s not an object initialized as a NumberFormat'");

