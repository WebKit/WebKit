description(
"Tests that a convert_this optimized for the case where this is an object behaves correctly when you then pass null."
);

function foo() {
    return this.x;
}

x = 42;

for (var i = 0; i < 200; ++i) {
    var me;
    if (i < 150)
        me = this;
    else
        me = null;
    shouldBe("foo.call(me)", "42");
}
