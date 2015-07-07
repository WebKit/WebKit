function f()
{
    function g() { }
}

for (var i = 0; i < 300000; ++i)
    f();
