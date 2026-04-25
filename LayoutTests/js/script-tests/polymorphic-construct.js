description(
"This tests that polymorphic construction works correctly."
);

function Foo() {
    this.field = "foo";
}

function Bar() {
    this.field = "bar";
}

function Baz() {
    this.field = "baz";
}

function construct(what) {
    return new what();
}

for (var i = 0; i < 3; ++i) {
    shouldBe("construct(Foo).field", "'foo'");
}

for (var i = 0; i < 3; ++i) {
    shouldBe("construct(Foo).field", "'foo'");
    shouldBe("construct(Bar).field", "'bar'");
    shouldBe("construct(Baz).field", "'baz'");
}
