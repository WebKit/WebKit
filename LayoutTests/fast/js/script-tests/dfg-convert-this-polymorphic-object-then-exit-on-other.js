description(
"Tests that a convert_this optimized for the case where this is an object with polymorphic structure behaves correctly when you then pass null."
);

function foo() {
    return this.x;
}

x = 42;

silentTestPass = true;
noInline(foo);

for (var i = 0; i < 1000; i = dfgIncrement({f:foo, i:dfgIncrement({f:foo, i:i + 1, n:100}), n:500, compiles:2})) {
    var me;
    if (i < 150)
        me = this;
    else if (i < 950)
        me = {x:42, y:62};
    else
        me = null;
    shouldBe("foo.call(me)", "42");
}
