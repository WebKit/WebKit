function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b)
{
    let v1 = a & b;
    let v2 = a | b;
    let v3 = a ^ b;
    return [v1, v2, v3];
}
noInline(test);

for (let i = 0; i < 1e4; ++i) {
    let [v1, v2, v3] = test(BigInt(i), BigInt(i + 1));
    shouldBe(v1, BigInt(i & (i + 1)));
    shouldBe(v2, BigInt(i | (i + 1)));
    shouldBe(v3, BigInt(i ^ (i + 1)));
}
let a = 0x7ffffffffn;
let b = 0xfff8302n;
let [v1, v2, v3] = test(a, b);
shouldBe(v1, a & b);
shouldBe(v2, a | b);
shouldBe(v3, a ^ b);
