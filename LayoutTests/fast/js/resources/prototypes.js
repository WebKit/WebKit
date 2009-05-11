description(
'Test prototoypes of various objects.'
);

shouldBe("('').__proto__", "String.prototype");
shouldBe("(0).__proto__", "Number.prototype");
shouldBe("([]).__proto__", "Array.prototype");
shouldBe("({}).__proto__", "Object.prototype");
shouldBe("(new Date).__proto__", "Date.prototype");
shouldBe("(new Number).__proto__", "Number.prototype");
shouldBe("(new Object).__proto__", "Object.prototype");
shouldBe("(new String).__proto__", "String.prototype");
shouldBe("Array.prototype.__proto__", "Object.prototype");
shouldBe("Date.prototype.__proto__", "Object.prototype");
shouldBe("Number.prototype.__proto__", "Object.prototype");
shouldBe("Object.prototype.__proto__", "null");
shouldBe("String.prototype.__proto__", "Object.prototype");
shouldBe("Array.__proto__", "Object.__proto__");
shouldBe("Date.__proto__", "Object.__proto__");
shouldBe("Number.__proto__", "Object.__proto__");
shouldBe("String.__proto__", "Object.__proto__");

var successfullyParsed = true;
