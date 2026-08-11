function test(base, get, set)
{
    Object.defineProperty(base, "hey", {
        get,
        set,
    });
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test({ }, function get() { return 42 }, function set() { });
