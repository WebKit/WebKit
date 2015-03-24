description(
"Tests that the specific value optimization does not break when the relevant structure is a dictionary."
)

function foo() {
    return x;
}

x = function() { };

var expected = "\"function () { }\"";

for (var i = 0; i < 1000; ++i) {
    eval("i" + i + " = " + i);
    if (i == 200) {
        x = 42;
        expected = "\"42\"";
    }
    shouldBe("\"\" + foo()", expected);
}
