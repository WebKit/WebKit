description(
"Tests that creating an indexed setter on the global object doesn't break things."
);

var thingy;

this.__defineSetter__(42, function(value) {
    thingy = value;
});

this[42] = "foo";

shouldBe("thingy", "\"foo\"");

