function test(base, value)
{
    Object.defineProperty(base, "hey", {
        value: value,
        writable: true,
    });
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test({ }, "property");
