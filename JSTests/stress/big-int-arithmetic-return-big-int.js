function fooAdd()
{
    return (1n+1n) / 1;
}
function fooSub()
{
    return (1n-1n) / 1;
}
function fooMul()
{
    return (1n*1n) / 1;
}
function fooDiv()
{
    return (1n/1n) / 1;
}

for (var i = 0; i < 10000 ; ++i) {
    try {
        fooAdd()
    } catch {}
    try {
        fooSub()
    } catch {}
    try {
        fooMul()
    } catch {}
    try {
        fooDiv()
    } catch {}
}
