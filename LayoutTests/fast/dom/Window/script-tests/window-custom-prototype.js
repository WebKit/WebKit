description("Test what happens when you set the window's prototype to various values.");

var originalWindowPrototype = this.__proto__;
var chainPointingBackToWindow = { __proto__: window };
var anotherObject = { };

shouldThrow("this.__proto__ = window; this.__proto__", "'TypeError: cyclic __proto__ value'");
shouldThrow("this.__proto__ = chainPointingBackToWindow; this.__proto__", "'TypeError: cyclic __proto__ value'");
shouldBe("this.__proto__ = 1; this.__proto__", "originalWindowPrototype");
shouldBe("this.__proto__ = 'a string'; this.__proto__", "originalWindowPrototype");
shouldBe("this.__proto__ = anotherObject; this.__proto__", "anotherObject");
shouldThrow("anotherObject.__proto__ = window; this.__proto__", "'TypeError: cyclic __proto__ value'");
shouldBe("this.__proto__ = 1; this.__proto__", "anotherObject");
shouldBe("this.__proto__ = 'a string'; this.__proto__", "anotherObject");
shouldBe("this.__proto__ = anotherObject; this.__proto__", "anotherObject");
shouldBe("this.__proto__ = originalWindowPrototype; this.__proto__", "originalWindowPrototype");
shouldBe("anotherObject.__proto__ = window; anotherObject.__proto__", "window");
shouldBe("this.__proto__ = null; this.__proto__", "undefined");
