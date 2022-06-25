description(
"Tests that we can inline a constructor that uses arguments without failing DFG validation."
);

function Foo() {
    this.x = arguments[0];
}

function bar() {
    return new Foo(42);
}

for (var i = 0; i < 200; ++i)
    shouldBe("bar().x", "42");
