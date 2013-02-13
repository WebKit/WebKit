description(
"Tests that our attempts to do peephole Branch(LogicalNot) optimization doesn't break OSR exit's ability to recover the value of LogicalNot."
);

function foo(a, b, c) {
    var x = a !== b;
    var result = a + c;
    if (x)
        result++;
    else
        result--;
    return result;
}

for (var i = 0; i < 200; ++i) {
    var expected = i + (i == 199 ? 2147483647 : 5) + (i !== 5 ? 1 : -1)
    shouldBe("foo(i, 5, i == 199 ? 2147483647 : 5)", "" + expected);
}

