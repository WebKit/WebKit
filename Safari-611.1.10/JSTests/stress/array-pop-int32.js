function foo(array) {
    return [array.pop(), array.pop(), array.pop(), array.pop()];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo([1, 2, 3]);
    if (result.toString() != "3,2,1,")
        throw "Error: bad result: " + result;
}
