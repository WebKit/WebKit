function foo()
{
    for (var i = 0; i < 100000; ++i)
        new Error();
}

function bar()
{
    foo();
}

function baz()
{
    bar();
}

baz();
