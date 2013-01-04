description(
"Tests that we do the right things when we prove that we can eliminate a structure check, but haven't proved that the value is definitely an object - i.e. we've proved that it's either an object with a specific structure, or it's not an object at all."
);

function foo(o, p) {
    var x = o.f;
    if (p)
        o = null;
    return x + o.g();
}

function bar() {
    return 24;
}

function baz(i) {
    try {
        return foo({f:42, g:bar}, i == 190);
    } catch (e) {
        debug("Caught exception: " + e);
        return "ERROR";
    }
}

for (var i = 0; i < 200; ++i)
    shouldBe("baz(i)", i == 190 ? "\"ERROR\"" : "66");
