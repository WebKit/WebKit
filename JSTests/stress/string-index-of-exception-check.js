//@ runDefault("--forceEagerCompilation=true")

// This test should not crash.

var count = 0;
function blurType(value)
{
    if ((count++) & 0x1)
        return {};
    return value;
}
noInline(blurType);

[0, 1].forEach(()=>{
    [{}, 1, 2].forEach(x => {
        ['xy'].indexOf(blurType('xy_'.substring(0, 2)));
    });
});
