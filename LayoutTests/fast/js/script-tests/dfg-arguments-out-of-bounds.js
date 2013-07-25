description(
"Tests accessing arguments with an out-of-bounds index when the arguments have not been created but might be."
);

var p = false;

function foo() {
    function bar() { }
    if (p)
        return arguments;
    return arguments[0];
}

silentTestPass = true;
noInline(foo);

var args = [42];
var expected = "\"42\"";
for (var i = 0; i < 3000; i = dfgIncrement({f:foo, i:i + 1, n:1500})) {
    if (i == 1000) {
        p = true;
        expected = "\"[object Arguments]\"";
    }
    if (i == 2000) {
        args = [];
        p = false;
        expected = "\"undefined\"";
    }
    result = "" +foo.apply(void 0, args);
    shouldBe("result", expected);
}

