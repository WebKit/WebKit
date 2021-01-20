function bar()
{
}

noInline(bar);

function foo(p, x)
{
    var value = 1;
    function inc()
    {
        return value + 1;
    }
    function dec()
    {
        return value - 1;
    }
    
    if (!p)
        return 0;
    
    bar(inc);
    
    x += 2000000000;
    
    value = 42;
    return dec();
}

noInline(foo);

function test(x)
{
    var result = foo(true, x);
    if (result != 42 - 1)
        throw "Error: bad result: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(0);

test(2000000000);
