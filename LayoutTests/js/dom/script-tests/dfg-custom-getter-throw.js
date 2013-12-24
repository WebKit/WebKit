description(
"Tests that DFG custom getter caching does not break the world if the getter throws an exception."
);

function foo(x) {
    return x.responseText;
}

function bar(binary) {
    var x = new XMLHttpRequest();
    x.open("GET", "http://foo.bar.com/");
    if (binary)
        x.responseType = "arraybuffer";
    try {
        return "Returned result: " + foo(x);
    } catch (e) {
        return "Threw exception: " + e;
    }
}

for (var i = 0; i < 200; ++i) {
    shouldBe("bar(i >= 100)", i >= 100 ? "\"Threw exception: Error: InvalidStateError: DOM Exception 11\"" : "\"Returned result: \"");
}


