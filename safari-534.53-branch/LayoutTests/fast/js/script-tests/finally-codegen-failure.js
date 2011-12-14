description("Test that finally behaviour is correct.");

a = {
    f: function() { return true; }
};

a.f.toString = function() { return "Fail"; };

function f() {
    try {
        a.f();
        a.f();
        return a.f();
    } finally {
        a.f();
    }
}
shouldBeTrue("f()")

shouldBeTrue("(function () { var a = true; try { return a; } finally { a = false; }})()");
shouldThrow("(function () { var a = 'PASS'; try { throw a; } finally { a = 'FAIL'; }})()");

successfullyParsed = true;
