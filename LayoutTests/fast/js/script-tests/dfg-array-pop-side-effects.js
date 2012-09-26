description(
"Tests that ArrayPop is known to the DFG to be a side effect."
);

function foo(a, b) {
    var result = a.f;
    result += b.pop();
    result += a.g;
    return result;
}

var ouches = 0;
for (var i = 0; i < 200; ++i) {
    var a = {f:1, g:2};
    var b = [];
    var expected;
    if (i < 150) {
        // Ensure that we always transition the array's structure to one that indicates
        // that we have array storage.
        b.__defineGetter__("0", function() {
            testFailed("Should never get here");
        });
        b.length = 0;
        b[0] = 42;
        expected = "45";
    } else {
        b.__defineGetter__("0", function() {
            debug("Ouch!");
            ouches++;
            delete a.g;
            a.h = 43;
            return 5;
        });
        expected = "0/0";
    }
    shouldBe("foo(a, b)", expected);
}

shouldBe("ouches", "50");
