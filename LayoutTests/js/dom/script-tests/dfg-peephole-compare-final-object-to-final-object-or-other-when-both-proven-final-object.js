description(
"Tests that the peephole CompareEq optimization for the case where one side is predicted final object and the other side is predicted either final object or other (i.e. null or undefined) doesn't assert when both sides are also proven final object."
);

function foo(a, b) {
    var result = [];
    result.push(a.f);
    result.push(b.f);
    if (a == b)
        result.push(true);
    else
        result.push(false);
    return result;
}

for (var i = 0; i < 100; ++i) {
    if (i%2) {
        var o = {f:42};
        shouldBe("foo(o, o)", "[42, 42, true]");
    } else
        shouldThrow("foo({f:42}, null)");
}

