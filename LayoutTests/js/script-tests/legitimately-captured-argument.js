description(
"Tests that using an argument as a captured variable, in the legitimate sense rather than the function.arguments sense, works as expected."
);

function makeCounter(x) {
    return function() {
        return ++x;
    };
}

for (var i = 0; i < 100; ++i) {
    var counter = makeCounter(i);
    for (var j = 0; j < 10; ++j)
        shouldBe("counter()", "" + (i + j + 1));
}
