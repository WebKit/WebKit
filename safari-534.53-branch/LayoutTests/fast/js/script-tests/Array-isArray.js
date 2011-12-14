description("Test to ensure correct behaviour of Array.array");

shouldBeTrue("Array.isArray([])");
shouldBeTrue("Array.isArray(new Array)");
shouldBeTrue("Array.isArray(Array())");
shouldBeTrue("Array.isArray('abc'.match(/(a)*/g))");
shouldBeFalse("(function(){ return Array.isArray(arguments); })()");
shouldBeFalse("Array.isArray()");
shouldBeFalse("Array.isArray(null)");
shouldBeFalse("Array.isArray(undefined)");
shouldBeFalse("Array.isArray(true)");
shouldBeFalse("Array.isArray(false)");
shouldBeFalse("Array.isArray('a string')");
shouldBeFalse("Array.isArray({})");
shouldBeFalse("Array.isArray({length: 5})");
shouldBeFalse("Array.isArray({__proto__: Array.prototype, length:1, 0:1, 1:2})");

successfullyParsed = true;
