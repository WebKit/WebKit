function foo(x) {
    var y;
    if (x===x)
        y = 42;
    else
        y = bar();
    return y + 1;
}

var result = 0;
var array = ["foo", 42, true, null, {}, [], foo];
for (var i = 0; i < 10000000; ++i)
    result += foo(array[i % array.length]);

if (result != (42 + 1) * 10000000)
    throw "Error";

