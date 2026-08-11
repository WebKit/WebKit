function test(base, set)
{
    Object.defineProperty(base, "hey", {
        set,
    });
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test({ }, function set(v) { });
