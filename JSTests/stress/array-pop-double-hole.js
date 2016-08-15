function foo() {
    var array = [1.5, 2, 3.5];
    array.pop();
    array[3] = 4.5;
    return array;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo();
    if (result.toString() != "1.5,2,,4.5")
        throw "Error: bad result: " + result;
}
