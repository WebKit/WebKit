var foo = function foo(unlikely) {
    if (unlikely)
        return eval("0");
    return 1;
}

noInline(foo);

function bar() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += foo(false);
    return result;
}

var result = bar();
if (result != 1000000)
    throw "Error: bad result: " + result;
