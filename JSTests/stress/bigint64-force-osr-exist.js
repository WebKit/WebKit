//@ runDefault("--useConcurrentJIT=0")
function shouldThrow(actual, expected) {
    function format(n) {
        return n + ":" + typeof n;
    }

    actual = format(actual);
    expected = format(expected);
    if (actual !== expected)
        throw new Error("bad! actual was " + actual + " but expect " + expected);
}

function sub(a, b) {
    return a - b;
}
noInline(sub);

for (var i = 0; i < 1e5; ++i) { }

{
    let a = -1n - 2n;
    let res = sub(a, 3n);
    shouldThrow(res, -6n);
}

{
    let a = -(0x7FFF_FFFF_FFFF_FFFFn + 1n);
    let res = sub(a, 3n);
    shouldThrow(res, -0x8000_0000_0000_0003n);
}

{
    let a = -(0x7FFF_FFFF_FFFF_FFFEn + 1n);
    let res = sub(a, 3n);
    shouldThrow(res, -0x8000_0000_0000_0002n);
}
