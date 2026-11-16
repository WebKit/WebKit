//@ runDefault("--useJIT=0")

function shouldBe(actual, expected, input)
{
    if (actual !== expected)
        throw new Error(`bad value for ${input}: ${actual}, expected ${expected}`);
}

// Reference ToInt32 through BigInt, independent of the conversion under test.
function toInt32Reference(x)
{
    if (!Number.isFinite(x))
        return 0;
    return Number(BigInt.asIntN(32, BigInt(Math.trunc(x))));
}

const inputs = [
    0, -0, 1, -1, 0.5, -0.5, 2147483647, -2147483648, 2147483648, -2147483649,
    2147483647.9, -2147483648.9, 0xc66363a5, 0xffffffff, 0x100000000, 0x100000001,
    4294967295.5, -4294967296, 2 ** 52 + 1, -(2 ** 52 + 1), 2 ** 53, 2 ** 53 + 2,
    2 ** 62, -(2 ** 62), 2 ** 63 - 1024, -(2 ** 63 - 1024), 2 ** 63, -(2 ** 63),
    2 ** 63 + 2048, -(2 ** 63 + 2048), 2 ** 64, 2 ** 84, 2 ** 85, 2 ** 100,
    Number.MAX_VALUE, -Number.MAX_VALUE, Number.MIN_VALUE, Infinity, -Infinity, NaN,
];

const int32Array = new Int32Array(1);
const uint32Array = new Uint32Array(1);

for (const input of inputs) {
    const expected = toInt32Reference(input);

    shouldBe(input | 0, expected, input);
    shouldBe(input ^ 0x5a5a5a5a, expected ^ 0x5a5a5a5a, input);
    shouldBe(input >> 0, expected, input);
    shouldBe(input >>> 0, expected >>> 0, input);

    int32Array[0] = input;
    shouldBe(int32Array[0], expected, input);

    uint32Array[0] = input;
    shouldBe(uint32Array[0], expected >>> 0, input);
}
