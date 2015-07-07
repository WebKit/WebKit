function foo(char) {
    switch (char) {
    case "a":
        return 1;
    case "b":
        return 2;
    case "c":
        return 3;
    default:
        return 4;
    }
}

function bar(array) {
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += foo(array[i & 3]);
    return result;
}

var result = bar(["a", "b", "c", "d"]);
if (result != 2500000)
    throw "Error: bad result: " + result;
