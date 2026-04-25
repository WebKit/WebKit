description("Tests for Array.of");

shouldBe("Array.of.length", "0");
shouldBe("Array.of.name", "'of'");

shouldBe("Array.of(1)", "[1]");
shouldBe("Array.of(1, 2)", "[1, 2]");
shouldBe("Array.of(1, 2, 3)", "[1, 2, 3]");

shouldBe("Array.of()", "[]");
shouldBe("Array.of(undefined)", "[undefined]");
shouldBe("Array.of('hello')", "['hello']");

debug("Construct nested Array with Array.of(Array.of(1, 2, 3), Array.of(4, 5, 6, 7, 8)).");
var x = Array.of(Array.of(1, 2, 3), Array.of(4, 5, 6, 7, 8));
shouldBe("x.length", "2");
shouldBe("x[0].length", "3");
shouldBe("x[1].length", "5");

debug("Check that a setter isn't called.");

Array.prototype.__defineSetter__("0", function (value) {
    throw new Error("This should not be called.");
});

shouldNotThrow("Array.of(1, 2, 3)");

debug("\"this\" is a constructor");
var Foo = function FooBar(length) { this.givenLength = length; };
shouldBeTrue("Array.of.call(Foo, 'a', 'b', 'c') instanceof Foo");
shouldBe("Array.of.call(Foo, 'a', 'b', 'c').givenLength", "3");
shouldBe("var foo = Array.of.call(Foo, 'a', 'b', 'c'); [foo.length, foo[0], foo[1], foo[2]]", "[3, 'a', 'b', 'c']");

debug("\"this\" is not a constructor");
var nonConstructorWasCalled = false;
var nonConstructor = () => { nonConstructorWasCalled = true; };
shouldBe("Array.of.call(nonConstructor, Foo).constructor", "Array");
shouldBe("Object.getPrototypeOf(Array.of.call(nonConstructor, Foo))", "Array.prototype");
shouldBe("Array.of.call(nonConstructor, Foo).length", "1");
shouldBe("Array.of.call(nonConstructor, Foo)[0]", "Foo");
shouldBeFalse("nonConstructorWasCalled");
