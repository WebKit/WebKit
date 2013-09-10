description(
"This tests that if the DFG fails to convert a uint32 to a number, it will OSR exit correctly."
);

function foo(a,b) {
    return a.f >>> b.f;
}

var result = 0;
for (var i = 0; i < 1000; ++i) {
    result += foo({f:i + 0.5}, {f:2});
}

shouldBe("result", "124500");

shouldBe("foo({f:2147483648}, {f:32})", "2147483648");
shouldBe("foo({f:2147483648}, {f:31})", "1");
shouldBe("foo({f:2147483648}, {f:30})", "2");

