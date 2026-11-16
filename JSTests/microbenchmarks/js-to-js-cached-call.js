function test() { }
noInline(test);

function cachedCallFromJS(func, times)
{
    for (var i = 0; i < times; ++i)
        func();
}
noInline(cachedCallFromJS);

cachedCallFromJS(test, 4e6);
