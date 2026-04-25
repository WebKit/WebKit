//@ $skipModes << :lockdown if $buildType == "debug"

globalThis.value = 42;

function test(v)
{
    globalThis.value = v;
}
noInline(test);

for (var i = 0; i < 1e8; ++i)
    test(i);
