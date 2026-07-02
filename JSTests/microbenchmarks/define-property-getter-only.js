function test(base, get)
{
    Object.defineProperty(base, "hey", {
        get,
    });
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test({ }, function get() { return 42 });
