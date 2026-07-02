description(
"Tests that right shifting by zero and then doing other bitops doesn't eliminate the ValueToInt32 node."
);

global = 32;

function f() {
    var Ha, fa;
    Ha = global >> 0;
    fa = Ha & 2;
    global = Ha & 2;
}

for (var i = 0; i < 1000; ++i)
    shouldBe("f()", "undefined");
