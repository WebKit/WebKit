description(
"Tests that if we emit a Flush of a GetLocal, we flush the source of the GetLocal."
);

function foo(a, b) {
    return a.f + a.g + b;
}

function fuzz(a, b) {
    if (a < b)
        return a - b;
    else
        return b - a;
}

function bar(a, b) {
    return foo({f:(a < b ? a - b : b - a), g:a}, b);
}

var result = 0;
for (var i = 0; i < 1000; ++i)
    result += bar(i, 1000 - i);

shouldBe("result", "500000");
