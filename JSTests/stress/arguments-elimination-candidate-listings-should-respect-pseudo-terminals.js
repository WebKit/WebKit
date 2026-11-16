var index = 0;
function sideEffect()
{
    return index++ === 0;
}
noInline(sideEffect);

function args(flag)
{
    var a = arguments;
    if (flag) {
        return a[4] + a[5];
    }
    return a.length;
}

function test(flag)
{
    args(flag, 0, 1, 2);
    if (sideEffect()) {
        OSRExit();
        args(sideEffect(), 0, 1, 2);
    }
}
noInline(test);

for (var i = 0; i < 1e3; ++i)
    test(false);
