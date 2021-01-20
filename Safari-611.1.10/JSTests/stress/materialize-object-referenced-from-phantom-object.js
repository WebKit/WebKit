function bar()
{
}

noInline(bar);

function foo(p, x)
{
    var a = {f: 1};
    var b = {f: a};
    var c = {f: a};
    
    if (!p)
        return 0;
    
    bar(b);
    
    x += 2000000000;
    
    c.f.f = 42;
    return b.f.f;
}

noInline(foo);

function test(x)
{
    var result = foo(true, x);
    if (result != 42)
        throw "Error: bad result: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(0);

test(2000000000);

