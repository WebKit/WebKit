description(
"Tests that DFG custom getter caching does not break the world if the getter throws an exception."
);

function foo(x) {
    return x.status;
}

function bar(doOpen) {
    var x = new XMLHttpRequest();
    if (doOpen)
        x.open("GET", "http://foo.bar.com/");
    try {
        return "Returned result: " + foo(x);
    } catch (e) {
        return "Threw exception: " + e;
    }
}

for (var i = 0; i < 200; ++i) {
    shouldBe("bar(i >= 100)", i >= 100 ? "\"Threw exception: Error: INVALID_STATE_ERR: DOM Exception 11\"" : "\"Returned result: 0\"");
}


