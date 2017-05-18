function foo()
{
    return arguments.length;
}

function bar(...args)
{
    var a = [42];
    if (isFinalTier())
        a = args;
    return {ftl: isFinalTier(), result: foo(...a)};
}

function baz()
{
    return bar(1, 2, 3, 4);
}

noInline(baz);

for (var i = 0; i < 100000; ++i) {
    var result = baz();
    if (result.ftl) {
        if (result.result != 4)
            throw "Error: bad result in loop in DFG: " + result;
    } else {
        if (result.result != 1)
            throw "Error: bad result in loop before DFG: " + result;
    }
}
