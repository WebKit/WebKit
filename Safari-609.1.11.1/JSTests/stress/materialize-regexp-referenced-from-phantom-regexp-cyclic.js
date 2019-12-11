function bar()
{
}

noInline(bar);

function foo(p, x)
{
    var a = /Hello/;
    a.lastIndex = 1;
    var b = /World/;
    b.lastIndex = a;
    var c = /World/;
    c.lastIndex = a;
    var d = /Cocoa/;
    d.lastIndex = c;
    a.lastIndex = d;

    if (!p)
        return 0;

    bar(b);

    x += 2000000000;

    c.lastIndex.lastIndex = 42;
    return b.lastIndex.lastIndex;
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

