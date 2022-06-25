description(
"Tests that a side-effecting ValueToInt32 only executes once when there is an OSR exit."
);

function foo(a, b) {
    var result = a | b.f;
    return [result, a];
}

var counter = 0;
for (var i = 0; i < 100; ++i) {
    var result = foo({valueOf:function() { counter++; return 1; }}, {f:i == 99 ? 5.5 : 5});
    shouldBe("result.length", "2");
    shouldBe("result[0]", "5");
}

shouldBe("counter", "100");
