//@ skip if $hostOS == "windows"
description("This test checks the behavior of the Intl object as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 8 The Intl Object

// The Intl object is a single ordinary object.
shouldBeType("Intl", "Object");
shouldBe("typeof Intl", "'object'");
shouldBe("Object.prototype.toString.call(Intl)", "'[object Object]'");

// The value of the [[Prototype]] internal slot of the Intl object is the intrinsic object %ObjectPrototype%.
shouldBeTrue("Object.getPrototypeOf(Intl) === Object.prototype");

// The Intl object is not a function object.
// It does not have a [[Construct]] internal method; it is not possible to use the Intl object as a constructor with the new operator.
shouldThrow("new Intl", "'TypeError: Object is not a constructor (evaluating \\'new Intl\\')'");

// The Intl object does not have a [[Call]] internal method; it is not possible to invoke the Intl object as a function.
shouldThrow("Intl()", "'TypeError: Intl is not a function. (In \\'Intl()\\', \\'Intl\\' is an instance of Object)'");

// Has only the built-in Collator, DateTimeFormat, and NumberFormat, which are not enumerable.
shouldBe("Object.keys(Intl).length", "0");

// Is deletable, inferred from use of "Initial" in spec, consistent with other implementations.
var __Intl = Intl;
shouldBeTrue("delete Intl;");

function global() { return this; }
shouldBeFalse("'Intl' in global()");

Intl = __Intl;

