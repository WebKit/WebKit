description(
"Tests that creating an indexed setter on the global object doesn't break things."
);

shouldThrowErrorName("this.__defineSetter__(42, function() {})", "TypeError");

this[42] = "foo";

shouldBe("this[42]", "undefined");

