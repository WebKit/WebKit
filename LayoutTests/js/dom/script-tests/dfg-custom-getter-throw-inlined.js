description(
"Tests that DFG custom getter caching does not break the world if the getter throws an exception from inlined code."
);

function foo(x) {
    return x.responseText;
}

function baz(x) {
    return foo(x);
}

function bar(binary) {
    var x = new XMLHttpRequest();
    x.open("GET", "http://foo.bar.com/");
    if (binary)
        x.responseType = "arraybuffer";
    try {
        return "Returned result: " + baz(x);
    } catch (e) {
        return "Threw exception: " + e;
    }
}

noInline(baz);
silentTestPass = true;

for (var i = 0; i < 200; i = dfgIncrement({f:baz, i:i + 1, n:50})) {
    shouldBe("bar(i >= 100)", i >= 100 ? "\"Threw exception: Error: InvalidStateError: DOM Exception 11\"" : "\"Returned result: \"");
}


