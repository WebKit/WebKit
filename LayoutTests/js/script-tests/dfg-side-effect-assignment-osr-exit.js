description(
"Tests what happens if we OSR exit on an assignment that was part of a side-effecting bytecode instruction."
);

function foo(f) {
    var x = f();
    if (x)
        return x;
}

var count = 0;
function bar() {
    count++;
    return eval(baz);
}

var baz = "42";

for (var i = 0; i < 500; ++i) {
    if (i == 450)
        baz = "\"stuff\"";
    shouldBe("foo(bar)", baz);
    shouldBe("count", "" + (i + 1));
}
