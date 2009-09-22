description(
"This test checks the behaviour of indexing an Array with immediate types."
);

var array = ["Zero", "One"];

shouldBe("array[0]", '"Zero"');
shouldBe("array[null]", "undefined");
shouldBe("array[undefined]", "undefined");
shouldBe("array[true]", "undefined");
shouldBe("array[false]", "undefined");

successfullyParsed = true;
