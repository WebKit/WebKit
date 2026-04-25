//@ $skipModes << :lockdown if $buildType == "debug"

function test(x)
{
    let r = 0;
    r += Array.isArray(x) ? 1 : 0;
    r += Array.isArray(x) ? 1 : 0;
    r += Array.isArray(x) ? 1 : 0;
    r += Array.isArray(x) ? 1 : 0;
    r += Array.isArray(x) ? 1 : 0;
    r += Array.isArray(x) ? 1 : 0;
    r += Array.isArray(x) ? 1 : 0;
    r += Array.isArray(x) ? 1 : 0;
    return r;
}
noInline(test);

let inputs = [[1, 2, 3], { length: 3 }, "abc", 42];
for (let i = 0; i < 1e6; ++i)
    test(inputs[i & 3]);
