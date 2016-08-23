function foo() {
    var result = 0;
    for (var i = 0; i < 1; ++i)
        result++;
    return result;
}

var result = 0;
for (var i = 0; i < 200000; ++i)
    result += foo();

if (result != 200000)
    throw "Bad result: " + result;



