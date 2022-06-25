description(
"Tests accessing arguments with an out-of-bounds index in an inlined function when the arguments have not been created but might be."
);

var p = false;

function foo() {
    if (p)
        return arguments;
    return arguments[0];
}

function bar() {
    return foo();
}

var expected = "\"undefined\"";
for (var i = 0; i < 3000; ++i) {
    if (i == 1000) {
        p = true;
        expected = "\"[object Arguments]\"";
    }
    if (i == 2000) {
        p = false;
        expected = "\"undefined\"";
    }
    result = "" + bar();
    shouldBe("result", expected);
}

