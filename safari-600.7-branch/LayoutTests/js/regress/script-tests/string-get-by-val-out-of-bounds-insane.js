function foo(string) {
    var result = ["", ""];
    for (var i = 0; i < 100000; ++i)
        result[i & 1] = string[i & 1];
    return result;
}

Object.prototype[1] = 42;

var result = foo("x");
if (result[0] != "x")
    throw "Bad result[0]: " + result[0];
if (result[1] != 42)
    throw "Bad result[1]: " + result[1];
