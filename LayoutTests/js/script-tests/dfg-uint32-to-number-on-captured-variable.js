description(
"Tests that storing the result of a uint32 to number conversion into a captured variable does not crash the compiler."
);

function foo(a) {
    var x = a >>> 0;
    return (function() {
        return x;
    });
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo(" + i + ")()", "" + i);

