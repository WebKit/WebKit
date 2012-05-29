description(
"Tests what happens if you OSR exit within inlined code that has interesting control flow with arguments that are specially formatted and you capture the arguments reflectively after the OSR exit."
);

function foo() {
    return bar.arguments[0];
}

function bar(x, y) {
    if (x + 5 > 4)
        return y.f + 42 + foo();
    else
        return y.f + 43 + foo();
}

function baz(x, y) {
    return bar(x, y);
}

for (var i = 0; i < 300; ++i) {
    var expected;
    var arg1 = i;
    var arg2;
    if (i < 250) {
        arg2 = {f:i + 1};
        expected = i + 1 + 42 + i;
    } else {
        arg2 = {f:1.5};
        expected = 1.5 + 42 + i;
    }
    shouldBe("baz(arg1, arg2)", "" + expected);
}

