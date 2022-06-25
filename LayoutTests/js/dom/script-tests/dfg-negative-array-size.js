description(
"Tests that creating an array with a negative size throws an exception."
);

function foo() {
    var totalLength = 0;

    for (var i = 1; i < 6000; ++i) {
        var j = (i > 4000) ? 2 : 0;
        var a = new Array(1 - j);

        if (a.length > 2147483647)
            break;

        totalLength += a.length;
    }

    return totalLength;
}

shouldThrow("foo()", undefined);


