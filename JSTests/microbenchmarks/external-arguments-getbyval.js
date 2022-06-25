//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(args)
{
    var result = 0;
    for (var i = 0; i < args.length; ++i)
        result += args[i];
    return result;
}

function bar()
{
    return foo(arguments);
}

var result = 0;
for (var i = 0; i < 10000; ++i)
    result += bar(i, i + 1, i + 2, i + 3);

if (result != 200040000)
    throw "Bad result: " + result;
