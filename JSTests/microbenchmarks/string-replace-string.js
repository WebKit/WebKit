//@ $skipModes << :lockdown if $buildType == "debug"

function test(a, b, c)
{
    return a.replace(b, c);
}
noInline(test);

for (var i = 0; i < 1e7; ++i)
    test("Hello World", "World", "Hi");
