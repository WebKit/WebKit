description("Test String.prototype.normalize.");

debug("Basic function properties");
shouldBeEqualToString("typeof String.prototype.normalize", "function");
shouldBeEqualToString("String.prototype.normalize.name", "normalize");
shouldBe("String.prototype.normalize.length", "0");
shouldBeTrue("Object.getOwnPropertyDescriptor(String.prototype, 'normalize').configurable");
shouldBeFalse("Object.getOwnPropertyDescriptor(String.prototype, 'normalize').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(String.prototype, 'normalize').writable");

debug("Invokes ToString on the argument.");
var listener = { callCount: 0, toString: function() { this.callCount++; return "WebKit"; } }
shouldBeEqualToString("String.prototype.normalize.call(listener)", "WebKit");
shouldBe("listener.callCount", "1");
