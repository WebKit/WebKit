function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " Actual: " + a);
}

function a(i) {
    "use strict";

    var foo;
    if (i) {
        var b = 3;
        foo = function () {
            return b;
        }
    } else {
        // Test if cachedVariablesUnderTDZIsEmpty is consistent with
        // cachedVariablesUnderTDZ
        foo = function () {
            return b;
        }

        function bar() {
            return b;
        }

        let b = 4;
        assert(bar(), 4);
    }

    return foo();
}

var b = 10;
assert(a(true), 3);
assert(a(false), 4);

