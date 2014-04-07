//@ runDefault

function foo() {
    var {0: i, 1: j, length} = arguments;
    return i + j + length;
}

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(i, 1);

if (result != 500002500000)
    throw "Bad result: " + result;
