function foo(string) {
    var result = ["", ""];
    for (var i = 0; i < 1000000; ++i)
        result[i & 1] = string[i & 1];
    return result;
}

var result = foo("x");
if (result[0] != "x")
    throw "Bad result[0]: " + result[0];
if (typeof result[1] != "undefined")
    throw "Bad result[1]: " + result[1];
