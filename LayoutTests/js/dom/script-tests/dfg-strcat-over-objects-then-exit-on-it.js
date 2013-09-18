description(
"Tests what happens when you do string concatentations on objects and then OSR exit when it turns out to be an int."
);

function foo() {
    return x;
}

function bar() {
    return foo() + '';
}

var x = function() { };

var expected = "\"function () { }\"";
var blah = this;
for (var i = 0; i < 1000; ++i) {
    blah["i" + i] = i;
    if (i == 700) {
        x = 42;
        expected = "\"42\"";
    }
    shouldBe("bar()", expected);
}

