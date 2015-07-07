description(
'This test checks for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=117281">117281: Crash in V8 benchmarks set in ARM,softfp,EABI.</a>'
);

function foo() {
    var a = [];

    for (var i = 0; i < 100000; ++i)
        a[i] = i + 0.5;

    var sum = 0;
    for (var i = 0; i < 100000; ++i)
        sum += a[i];

    return sum;
}

var result = foo();

shouldBe("result", "5000000000");
