description(
"Tests that a switch statement with a negative integer doesn't cause weirdness."
);

function foo(x) {
    switch (x) {
    case -1:
        return "foo";
    case 0:
        return "bar";
    case 1:
        return "baz";
    }
}

noInline(foo);
while (!dfgCompiled({f:foo})) {
    for (var i = -1; i <= 1; ++i)
        foo(i);
}

shouldBe("foo(-1)", "\"foo\"");
shouldBe("foo(0)", "\"bar\"");
shouldBe("foo(1)", "\"baz\"");
