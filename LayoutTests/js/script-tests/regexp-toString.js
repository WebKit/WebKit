description('Test RegExp#toString');

shouldBe("Object.getOwnPropertyDescriptor(RegExp.prototype, 'toString').configurable", "true");
shouldBe("Object.getOwnPropertyDescriptor(RegExp.prototype, 'toString').enumerable", "false");
shouldBe("Object.getOwnPropertyDescriptor(RegExp.prototype, 'toString').get", "undefined");
shouldBe("Object.getOwnPropertyDescriptor(RegExp.prototype, 'toString').set", "undefined");
shouldBe("typeof Object.getOwnPropertyDescriptor(RegExp.prototype, 'toString').value", "'function'");

shouldBe("RegExp.prototype.toString.call(new RegExp)", "'/(?:)/'");
shouldBe("RegExp.prototype.toString.call(new RegExp('a'))", "'/a/'");
shouldBe("RegExp.prototype.toString.call(new RegExp('\\\\\\\\'))", "'/\\\\\\\\/'");

shouldBe("RegExp.prototype.toString.call({})", "'/undefined/'");
shouldBe("RegExp.prototype.toString.call({source: 'hi'})", "'/hi/'");
shouldBe("RegExp.prototype.toString.call({ __proto__: { source: 'yo' } })", "'/yo/'");
shouldBe("RegExp.prototype.toString.call({source: ''})", "'//'");
shouldBe("RegExp.prototype.toString.call({source: '/'})", "'///'");

shouldThrow("RegExp.prototype.toString.call(undefined)");
shouldThrow("RegExp.prototype.toString.call(null)");
shouldThrow("RegExp.prototype.toString.call(false)");
shouldThrow("RegExp.prototype.toString.call(true)");
shouldThrow("RegExp.prototype.toString.call(0)");
shouldThrow("RegExp.prototype.toString.call(0.5)");
shouldThrow("RegExp.prototype.toString.call('x')");
