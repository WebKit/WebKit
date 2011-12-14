description("Test what happens when you set the window's prototype to various values.");

var originalWindowPrototype = __proto__;
var chainPointingBackToWindow = { __proto__: window };
var anotherObject = { };

shouldThrow("__proto__ = window; __proto", "'Error: cyclic __proto__ value'");
shouldThrow("__proto__ = chainPointingBackToWindow; __proto__", "'Error: cyclic __proto__ value'");
shouldBe("__proto__ = 1; __proto__", "originalWindowPrototype");
shouldBe("__proto__ = 'a string'; __proto__", "originalWindowPrototype");
shouldBe("__proto__ = anotherObject; __proto__", "anotherObject");
shouldThrow("anotherObject.__proto__ = window; __proto__", "'Error: cyclic __proto__ value'");
shouldBe("__proto__ = null; __proto__", "null");
shouldBe("__proto__ = 1; __proto__", "null");
shouldBe("__proto__ = 'a string'; __proto__", "null");
shouldBe("__proto__ = anotherObject; __proto__", "anotherObject");
shouldBe("__proto__ = originalWindowPrototype; __proto__", "originalWindowPrototype");
shouldBe("anotherObject.__proto__ = window; anotherObject.__proto__", "window");

var successfullyParsed = true;
