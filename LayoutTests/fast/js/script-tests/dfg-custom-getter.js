description(
"Tests that DFG custom getter caching does not break the world."
);

function foo() {
    var result = '';
    for (var current = document.getElementById('myFirstDiv'); current; current = current.nextSibling)
        result += ' ' + current.nodeName;
    return result;
}

for (var i = 0; i < 200; ++i)
    shouldBe("foo()", "\" DIV #text DIV #text SCRIPT\"");

