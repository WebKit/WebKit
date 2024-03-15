function opt(v) {
    let a = (-1 >>> v)
    let b = (a + -0.2)
    let c = b | 0
    return c
}
noInline(opt)

function opt2() {
    var x = -19278.05 >>> NaN; // 4294948018
    var y = x + -19278.05;     // 4294928739.95
    var z = y >> 0;            // -38557
    return z;
}
noInline(opt2);

function o(v) {
    opt(v)
}
noInline(o)

{
    for (let i = 0; i < 10000; i++)
        o(0)
    for (let i = 0; i < 10000; ++i)
        opt(32)
    if (opt(32) != -2)
        throw "wrong value"
}

{
    let a = opt(0);
    for (let i = 0; i < 1e3; i++) {
        let b = opt(0);
        if (a !== b)
            throw 'Expected ' + a + ' but got ' + b;
    }
}

{
    let a = opt();
    for (let i = 0; i < 1e3; i++) {
        let b = opt();
        if (a !== b)
            throw 'Expected ' + a + ' but got ' + b;
    }
}
