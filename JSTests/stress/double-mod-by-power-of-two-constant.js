function modBy1(x) { return x % 1; }
function modBy2(x) { return x % 2; }
function modBy8(x) { return x % 8; }
function modByMinus4(x) { return x % -4; }
function modByHalf(x) { return x % 0.5; }
function modBy2p52(x) { return x % 4503599627370496; }
function modBy2p53(x) { return x % 9007199254740992; }
function modBy2p1023(x) { return x % 8.98846567431158e+307; }
function modByMinus2p31(x) { return x % -2147483648; }
function modBy2p31(x) { return x % 2147483648; }
function modBy2p32(x) { return x % 4294967296; }
function modBy3(x) { return x % 3; }
noInline(modBy1);
noInline(modBy2);
noInline(modBy8);
noInline(modByMinus4);
noInline(modByHalf);
noInline(modBy2p52);
noInline(modBy2p53);
noInline(modBy2p1023);
noInline(modByMinus2p31);
noInline(modBy2p31);
noInline(modBy2p32);
noInline(modBy3);

function reference(x, y) { return x % y; }
noInline(reference);

const tests = [
    [modBy1, 1],
    [modBy2, 2],
    [modBy8, 8],
    [modByMinus4, -4],
    [modByHalf, 0.5],
    [modBy2p52, 4503599627370496],
    [modBy2p53, 9007199254740992],
    [modBy2p1023, 8.98846567431158e+307],
    [modByMinus2p31, -2147483648],
    [modBy2p31, 2147483648],
    [modBy2p32, 4294967296],
    [modBy3, 3],
];

const inputs = [
    0, -0, 0.5, -0.5, 1, -1, 1.5, -1.5, 2, -2, 3.75, -3.75, 4, -4, 7.999999999999999, -8, 8.000000000000002,
    0.1, -0.1, 1e-310, -1e-310, 5e-324, -5e-324, 2.2250738585072014e-308, -2.2250738585072014e-308,
    123456789.123, -123456789.123, 4503599627370495.5, -4503599627370495.5, 4503599627370497, -4503599627370497,
    9007199254740991, -9007199254740991, 9007199254740994, -9007199254740994, 1e300, -1e300, 1.7976931348623157e308,
    -1.7976931348623157e308, Infinity, -Infinity, NaN,
    -3, 5, -7, 9, -16, 1073741824, -1073741824, 2147483647, -2147483647, -2147483648, 2147483648, -2147483649,
];

let seed = 1;
function random() {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    return seed / 2147483648;
}
for (let i = 0; i < 100; ++i)
    inputs.push((random() - 0.5) * Math.pow(2, Math.floor(random() * 120) - 40));

for (let i = 0; i < testLoopCount; ++i) {
    const input = inputs[i % inputs.length];
    for (const [func, divisor] of tests) {
        const actual = func(input);
        const expected = reference(input, divisor);
        if (!Object.is(actual, expected))
            throw new Error(`${func.name}(${input}): expected ${expected} but got ${actual}`);
    }
}

function modBy4Generic(x) { return x % 4; }
noInline(modBy4Generic);

const genericInputs = ["-6", "7.5", true, null, undefined, { valueOf() { return -12.5; } }, -8, 2.5];
for (let i = 0; i < testLoopCount; ++i) {
    const input = genericInputs[i % genericInputs.length];
    const actual = modBy4Generic(input);
    const expected = reference(input, 4);
    if (!Object.is(actual, expected))
        throw new Error(`modBy4Generic(${String(input)}): expected ${expected} but got ${actual}`);
}
