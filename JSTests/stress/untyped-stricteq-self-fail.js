function bar() { return 10; }

function foo(x) {
    var y;
    if (x===x)
        y = 42;
    else
        y = bar();
    return y + 1;
}

noInline(foo);

var result = 0;
var array = ["foo", 42, true, null, {}, [], foo];
for (var i = 0; i < 1000000; ++i)
    result += foo(array[i % array.length]);

if (result != (42 + 1) * 1000000)
    throw "Error";

var resultAtEnd = foo(0.0 / 0.0);
if (resultAtEnd != 11)
    throw "Error at end";
