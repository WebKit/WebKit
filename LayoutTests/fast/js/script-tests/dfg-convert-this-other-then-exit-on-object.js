description(
"Tests that a convert_this optimized for the case where this is null behaves correctly when you then pass an object."
);

function foo() {
    return this.x;
}

x = 42;

for (var i = 0; i < 200; ++i) {
    var me;
    if (i < 150)
        me = null;
    else
        me = this;
    shouldBe("foo.call(me)", "42");
}
