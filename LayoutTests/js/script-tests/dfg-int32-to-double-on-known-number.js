description(
"Tests that we don't emit unnecessary speculation checks when performing an int32 to double conversion on a value that is proved to be a number, predicted to be an int32, but not proved to be an int32."
);

function foo(a, b) {
    var x = a.f;
    var y;
    function bar() {
        return y;
    }
    var z = x + b;
    y = x;
    if (z > 1)
        return y + b + bar();
    else
        return 72;
}

for (var i = 0; i < 200; ++i)
    shouldBe("foo({f:5}, 42.5)", "52.5");
