description(
"Check that short-circuiting Branch(LogicalNot(@a)) and then failing speculation does not result in the branch being taken the wrong way."
);

function foo(a) {
    if (a.f)
        return 1;
    return 0;
}

function bar(a) {
    var b = !a.f;
    if (b)
        return 1;
    return 0;
}

var True = true;
var False = false;
for (var i = 0; i < 200; ++i) {
    var x;
    if (i == 100) {
        True = "string";
        False = void 0;
    }
    shouldBe("foo({f:True})", "1");
    shouldBe("foo({f:False})", "0");
    shouldBe("bar({f:True})", "0");
    shouldBe("bar({f:False})", "1");
}

