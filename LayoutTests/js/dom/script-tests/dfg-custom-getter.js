description(
"Tests that DFG custom getter caching does not break the world."
);

function foo() {
    var result = '';
    for (var current = document.getElementById('myFirstDiv'); current; current = current.nextSibling)
        result += ' ' + current.nodeName;
    return result;
}

dfgShouldBe(foo, "foo()", "\" DIV #text DIV #text SCRIPT\"");

