description(
"Tests that the CompareEq optimization for the case where one side is predicted final object and the other side is predicted either final object or other (i.e. null or undefined) doesn't assert when both sides are also proven final object."
);

function foo(a, b) {
    return [a.f, b.f, a == b];
}

silentTestPass = true;
noInline(foo);

for (var i = 0; i < 100; i = dfgIncrement({f:foo, i:i + 1, n:50})) {
    if (i%2) {
        var o = {f:42};
        shouldBe("foo(o, o)", "[42, 42, true]");
    } else
        shouldThrow("foo({f:42}, null)");
}

