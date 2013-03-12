description(
"Tests that when values predicted but not proven int are used in a tower of additions, we don't eliminate the overflow check unsoundly."
);

function foo(a, b, o) {
    return (a + b + o.f) | 0;
}

function bar(a, b, o) {
    eval(""); // Prevent this function from being opt compiled.
    return foo(a, b, o);
}

var badCases = [
    {a:2147483645, b:2147483644, c:9007199254740990, expected:-8},
    {a:2147483643, b:2147483643, c:18014398509481980, expected:-16},
    {a:2147483643, b:2147483642, c:36028797018963960, expected:-16},
    {a:2147483642, b:2147483642, c:36028797018963960, expected:-16},
    {a:2147483641, b:2147483640, c:144115188075855840, expected:-32},
    {a:2147483640, b:2147483640, c:144115188075855840, expected:-64},
    {a:2147483640, b:2147483639, c:288230376151711680, expected:-64},
    {a:2147483639, b:2147483639, c:288230376151711680, expected:-64}
];

var warmup = 100;

for (var i = 0; i < warmup + badCases.length; ++i) {
    var a, b, c;
    var expected;
    if (i < warmup) {
        a = 1;
        b = 2;
        c = 3;
        expected = 6;
    } else {
        var current = badCases[i - warmup];
        a = current.a;
        b = current.b;
        c = current.c;
        expected = current.expected;
    }
    shouldBe("bar(" + a + ", " + b + ", {f:" + c + "})", "" + expected);
}

