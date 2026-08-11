"use strict"

description("Verify the various properties of String.prototype[Symbol.iterator]");


debug("Iterator object properties");
shouldBeEqualToString("typeof String.prototype[Symbol.iterator]", "function");
shouldBeEqualToString("String.prototype[Symbol.iterator].name", "[Symbol.iterator]");
shouldBe("String.prototype[Symbol.iterator].length", "0");
shouldBeTrue("Object.getOwnPropertyDescriptor(String.prototype, Symbol.iterator).configurable");
shouldBeFalse("Object.getOwnPropertyDescriptor(String.prototype, Symbol.iterator).enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(String.prototype, Symbol.iterator).writable");
shouldBeFalse("String.prototype[Symbol.iterator]() === String.prototype[Symbol.iterator]()");

debug("Iterating a simple string.");
let iterator = "WebKit"[Symbol.iterator]();

let next = iterator.next();
shouldBeEqualToString("next.value", "W");
shouldBeFalse("next.done");
next = iterator.next();
shouldBeEqualToString("next.value", "e");
shouldBeFalse("next.done");
next = iterator.next();
shouldBeEqualToString("next.value", "b");
shouldBeFalse("next.done");
next = iterator.next();
shouldBeEqualToString("next.value", "K");
shouldBeFalse("next.done");
next = iterator.next();
shouldBeEqualToString("next.value", "i");
shouldBeFalse("next.done");
next = iterator.next();
shouldBeEqualToString("next.value", "t");
shouldBeFalse("next.done");
next = iterator.next();
shouldBe("next.value", "undefined");
shouldBeTrue("next.done");
next = iterator.next();
shouldBe("next.value", "undefined");
shouldBeTrue("next.done");
