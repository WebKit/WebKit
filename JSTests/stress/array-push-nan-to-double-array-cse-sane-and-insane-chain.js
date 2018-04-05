function foo(array, otherArray, i)
{
    var x = otherArray[i];
    var y = otherArray[i];
    array.push(y);
    return x / 42;
}

function bar()
{
    return [];
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 10000; ++i)
    foo(bar(), [42.5], 0);

var result = bar();
foo(result, [,42.5], 0);
if (result[0] !== void 0)
    throw "Bad result: " + result;
