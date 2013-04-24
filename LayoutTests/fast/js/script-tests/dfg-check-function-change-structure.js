function foo(o) {
    var f = o[0];
    return [f(), f.f];
}

function bar() {
    return 42;
}

bar.f = 23;

var expected = "[42, 23]";

for (var i = 0; i < 100; ++i) {
    if (i == 95) {
        delete bar.f;
        bar.g = 36;
        expected = "[42, void 0]";
    }
    shouldBe("foo([bar])", expected);
}
