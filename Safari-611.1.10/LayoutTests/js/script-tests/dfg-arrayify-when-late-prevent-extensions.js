description(
"Tests that Arraify does good things when Object.preventExtensions() has been called."
);

function foo(o) {
    o[0] = 42;
    return o[0];
}

silentTestPass = true;
noInline(foo);

for (var i = 0; i < 200; i = dfgIncrement({f:foo, i:i + 1, n: 100})) {
    var o = {};
    var expected;
    if (i >= 150) {
        Object.preventExtensions(o);
        expected = "void 0";
    } else
        expected = "42";
    shouldBe("foo(o)", expected);
}
