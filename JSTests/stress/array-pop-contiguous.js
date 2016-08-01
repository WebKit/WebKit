function foo(array) {
    return [array.pop(), array.pop(), array.pop(), array.pop()];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(["foo", "bar", "baz"]);
    if (result.toString() != "baz,bar,foo,")
        throw "Error: bad result: " + result;
}
