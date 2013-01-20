function foo(a) {
    var result = 0;
    for (var i = 0 ; i < 5000000; ++i)
        result += a.valueOf();
    return result;
}

var result = foo(5);

if (result != 25000000) {
    print("Bad result: " + result);
    throw "Bad result: " + result;
}
