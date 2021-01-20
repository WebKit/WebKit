description("document.all works as superclass.prototype, but not as superclass");

function testAsSuperclassPrototype() {
    function Bar() {}
    Bar.prototype = document.all;

    for (let i = 0; i < 1e5; ++i) {
        class Foo extends Bar {}

        if (!(new Foo() instanceof Bar))
            return false;
    }

    return true;
}

shouldBe("testAsSuperclassPrototype()", "true");
shouldThrow("class Foo extends document.all {}", "'TypeError: The superclass is not a constructor.'");
