description(
"Tests that Arraify does good things when Object.preventExtensions() has been called."
);

function foo(o) {
    o[0] = 42;
    return o[0];
}

dfgShouldBe(foo, "var o = {}; Object.preventExtensions(o); foo(o)", "void 0");
