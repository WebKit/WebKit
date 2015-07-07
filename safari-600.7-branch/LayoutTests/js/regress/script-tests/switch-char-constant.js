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

function bar() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += foo("a");
    return result;
}

var result = bar();
if (result != 1000000)
    throw "Error: bad result: " + result;
