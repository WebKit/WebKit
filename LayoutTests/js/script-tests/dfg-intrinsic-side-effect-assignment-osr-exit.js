description(
"Tests what happens if we OSR exit on an assignment that was part of a side-effecting intrinsic."
);

function foo(array) {
    var x = array.pop();
    if (x)
        return x;
}

for (var i = 0; i < 500; ++i) {
    var array = [];
    if (i == 50)
        array.x = 42;
    array.push("blah");
    var expected;
    if (i >= 450) {
        array.push(2);
        expected = "2";
    } else {
        array.push("bleh");
        expected = "\"bleh\"";
    }
    shouldBe("foo(array)", expected);
}

