description("This test checks the behavior of Intl.DateTimeFormat as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 12.1 The Intl.DateTimeFormat Constructor

// The Intl.DateTimeFormat constructor is a standard built-in property of the Intl object.
shouldBeType("Intl.DateTimeFormat", "Function");

// 12.1.2 Intl.DateTimeFormat([ locales [, options]])
shouldBeType("Intl.DateTimeFormat()", "Intl.DateTimeFormat");
shouldBeType("Intl.DateTimeFormat.call({})", "Intl.DateTimeFormat");
shouldBeType("new Intl.DateTimeFormat()", "Intl.DateTimeFormat");

// Subclassable
var classPrefix = "class DerivedDateTimeFormat extends Intl.DateTimeFormat {};";
shouldBeTrue(classPrefix + "(new DerivedDateTimeFormat) instanceof DerivedDateTimeFormat");
shouldBeTrue(classPrefix + "(new DerivedDateTimeFormat) instanceof Intl.DateTimeFormat");
shouldBeTrue(classPrefix + "new DerivedDateTimeFormat().format(0).length > 0");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(new DerivedDateTimeFormat) === DerivedDateTimeFormat.prototype");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(Object.getPrototypeOf(new DerivedDateTimeFormat)) === Intl.DateTimeFormat.prototype");

// 12.2 Properties of the Intl.DateTimeFormat Constructor

// length property (whose value is 0)
shouldBe("Intl.DateTimeFormat.length", "0");

// 12.2.1 Intl.DateTimeFormat.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').enumerable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').configurable");

// 12.2.2 Intl.DateTimeFormat.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.length", "1");

// Returns SupportedLocales
shouldBeType("Intl.DateTimeFormat.supportedLocalesOf()", "Array");
// Doesn't care about `this`.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.call(null, 'en')", "[ 'en' ]");
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.call({}, 'en')", "[ 'en' ]");
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.call(1, 'en')", "[ 'en' ]");
// Ignores non-object, non-string list.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf(9)", "[]");
// Makes an array of tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('en')", "[ 'en' ]");
// Handles array-like objects with holes.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })", "[ 'es', 'en', 'de' ]");
// Deduplicates tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])", "[ 'en', 'pt', 'es' ]");
// Canonicalizes tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('En-laTn-us-variant2-variant1-1abc-U-ko-tRue-A-aa-aaa-x-RESERVED')", "[ 'en-Latn-US-variant2-variant1-1abc-a-aa-aaa-u-ko-true-x-reserved' ]");
// Replaces outdated tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('no-bok')", "[ 'nb' ]");
// Doesn't throw, but ignores private tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('x-some-thing')", "[]");
// Throws on problems with length, get, or toString.
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('')", "'RangeError: invalid language tag: '");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('a')", "'RangeError: invalid language tag: a'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en--US')", "'RangeError: invalid language tag: en--US'");

// 12.3 Properties of the Intl.DateTimeFormat Prototype Object

// The value of Intl.DateTimeFormat.prototype.constructor is %DateTimeFormat%.
shouldBe("Intl.DateTimeFormat.prototype.constructor", "Intl.DateTimeFormat");

// 12.3.3 Intl.DateTimeFormat.prototype.format

// This named accessor property returns a function that formats a date according to the effective locale and the formatting options of this DateTimeFormat object.
shouldBeType("Intl.DateTimeFormat.prototype.format", "Function");

// The value of the [[Get]] attribute is a function
shouldBeType("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').get", "Function");

// The value of the [[Set]] attribute is undefined.
shouldBe("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').set", "undefined");

// Match Firefox where unspecifed.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').configurable");

// The value of F’s length property is 1.
shouldBe("Intl.DateTimeFormat.prototype.format.length", "1");

// Throws on non-DateTimeFormat this.
shouldThrow("Object.defineProperty({}, 'format', Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format')).format", "'TypeError: Intl.DateTimeFormat.prototype.format called on value that\\'s not an object initialized as a DateTimeFormat'");

// The format function is unique per instance.
shouldBeTrue("Intl.DateTimeFormat.prototype.format !== Intl.DateTimeFormat().format");
shouldBeTrue("new Intl.DateTimeFormat().format !== new Intl.DateTimeFormat().format");

// 12.3.4 DateTime Format Functions

// 1. Let dtf be the this value.
// 2. Assert: Type(dtf) is Object and dtf has an [[initializedDateTimeFormat]] internal slot whose value is true.
// This should not be reachable, since format is bound to an initialized datetimeformat.

// 3. If date is not provided or is undefined, then
// a. Let x be %Date_now%().
// 4. Else
// a. Let x be ToNumber(date).
// b. ReturnIfAbrupt(x).
shouldThrow("Intl.DateTimeFormat.prototype.format({ valueOf() { throw Error('4b') } })", "'Error: 4b'");

// 12.3.4 FormatDateTime abstract operation

// 1. If x is not a finite Number, then throw a RangeError exception.
shouldThrow("Intl.DateTimeFormat.prototype.format(Infinity)", "'RangeError: date value is not finite in DateTimeFormat.format()'");

// Format is bound, so calling with alternate "this" has no effect.
shouldBe("Intl.DateTimeFormat.prototype.format.call(null, 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("Intl.DateTimeFormat.prototype.format.call(Intl.DateTimeFormat('ar'), 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("Intl.DateTimeFormat.prototype.format.call(5, 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("new Intl.DateTimeFormat().format.call(null, 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("new Intl.DateTimeFormat().format.call(Intl.DateTimeFormat('ar'), 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("new Intl.DateTimeFormat().format.call(5, 0)", "Intl.DateTimeFormat().format(0)");

// 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions ()

shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions.length", "0");

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBeType("Intl.DateTimeFormat.prototype.resolvedOptions()", "Object");

// Returns a new object each time.
shouldBeFalse("Intl.DateTimeFormat.prototype.resolvedOptions() === Intl.DateTimeFormat.prototype.resolvedOptions()");

// Throws on non-DateTimeFormat this.
shouldThrow("Intl.DateTimeFormat.prototype.resolvedOptions.call(5)", "'TypeError: Intl.DateTimeFormat.prototype.resolvedOptions called on value that\\'s not an object initialized as a DateTimeFormat'");

