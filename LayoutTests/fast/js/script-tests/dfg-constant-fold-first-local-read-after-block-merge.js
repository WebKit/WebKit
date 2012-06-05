description(
"Tests that the DFG doesn't crash if we constant fold the first read of a local variable in a block that is the result of merging two blocks, where the first doesn't touch the local and the second reads it."
);

function foo(x, y) {
    var o = {};
    if (y == 5) {
        o.f = 42;
    }
    var z = o.f;
    if (x == 5) {
        if (y == 5) {
            return z;
        }
    }
}

function bar(x) {
    return foo(x, 5);
}

for (var i = 0; i < 200; ++i)
    shouldBe("bar(5)", "42");

