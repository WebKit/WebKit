// This feature isn't enabled yet.
//@ skip

description("This test checks the behavior of the Intl object as described in 8 of the ECMAScript Internationalization API Specification.");

// The Intl object is a standard built-in object that is the initial value of the Intl property of the global object.
shouldBeType("Intl", "Object");
shouldBe("typeof Intl", "'object'");
shouldBe("Object.prototype.toString.call(Intl)", "'[object Object]'");

// The value of the [[Prototype]] internal property of the Intl object is the built-in Object prototype object specified by the ECMAScript Language Specification.
shouldBeTrue("Object.getPrototypeOf(Intl) === Object.prototype");

// The Intl object does not have a [[Construct]] internal property; it is not possible to use the Intl object as a constructor with the new operator.
shouldThrow("new Intl", "'TypeError: Object is not a constructor (evaluating \\'new Intl\\')'");

// The Intl object does not have a [[Call]] internal property; it is not possible to invoke the Intl object as a function.
shouldThrow("Intl()", "'TypeError: Intl is not a function. (In \\'Intl()\\', \\'Intl\\' is an instance of Object)'");

// is deletable, inferred from use of "Initial" in spec, consistent with other implementations
var __Intl = Intl;
shouldBeTrue("delete Intl;");

function global() { return this; }
shouldBeFalse("'Intl' in global()");

Intl = __Intl;
