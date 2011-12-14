description(

"This test checks that methods called directly on primitive types get the wrapper, not the primitive, as the 'this' object."

);


String.prototype.thisType = function() { return typeof this; };
Number.prototype.thisType = function() { return typeof this; };
Boolean.prototype.thisType = function() { return typeof this; };

shouldBe("(1).thisType()", "'object'");
shouldBe("(2.3).thisType()", "'object'");
shouldBe("'xxx'.thisType()", "'object'");
shouldBe("(false).thisType()", "'object'");

successfullyParsed = true;
