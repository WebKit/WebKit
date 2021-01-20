function foo(array, num, denum)
{
    array.push(num/denum);
}

function bar()
{
    return [];
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 10000; ++i)
    foo(bar(), 42.5, 3.1);

var result = bar();
foo(result, 0, 0);
if ("" + result[0] !== "NaN")
    throw "Bad result: " + result;
