description("This test ensures that we will call a custom setter when the setter is in the prototype chain");

// The RegExp Object is a convenient constructor with a custom setter.
// This will also test the weird behaviour in which the RegExp constructor
// setters are expected to act on the base object (e.g. the constructor object)
// rather than the this value.
var testObject = {__proto__: RegExp }

testObject.input = "testInput"

shouldBe("testObject.input", "'testInput'")
shouldBe("RegExp.input", "'testInput'")
