var toggle = 0;

function bar()
{
    if (toggle ^= 1)
        return 42;
    else
        return {valueOf: function() { return 42; }};
}

noInline(bar);

function baz()
{
    return 7;
}

noInline(baz);

function foo()
{
    return bar() ^ baz();
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo();
    if (result != 45)
        throw "Error: bad result: " + result;
}
