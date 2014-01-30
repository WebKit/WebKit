function foo(array) {
    return [array.pop(), array.pop(), array.pop(), array.pop()];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo([1.5, 2.5, 3.5]);
    if (result.toString() != "3.5,2.5,1.5,")
        throw "Error: bad result: " + result;
}
