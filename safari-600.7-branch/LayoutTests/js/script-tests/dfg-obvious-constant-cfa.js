description(
"This tests that if the CFA performs constant folding on an obvious set and then get of a captured local, then we don't crash."
);

function bar(a, b) {
    var x;
    var y;
    function baz() {
        return x + y;
    }
    x = 13;
    y = 16;
    if (y == 16) {
        return x + a + b + baz();
    } else
        return 24;
}

var result = 0;
for (var i = 0; i < 200; ++i)
    result += bar(i, 1000);

shouldBe("result", "228300");
