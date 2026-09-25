function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function sum(o) {
    return o.f0 + o.f1 + o.f2 + o.f3 + o.f4 + o.f5 + o.f6 + o.f7 + o.f8 + o.f9
        + o.f10 + o.f11 + o.f12 + o.f13 + o.f14 + o.f15 + o.f16 + o.f17 + o.f18 + o.f19
        + o.f20 + o.f21 + o.f22 + o.f23 + o.f24 + o.f25 + o.f26 + o.f27 + o.f28 + o.f29
        + o.f30 + o.f31 + o.f32 + o.f33 + o.f34 + o.f35 + o.f36 + o.f37 + o.f38 + o.f39;
}
noInline(sum);

const fieldCount = 40;

function make(index, value) {
    let o = { };
    for (let i = 0; i < fieldCount; ++i)
        o["f" + i] = i;
    if (index >= 0)
        o["f" + index] = value;
    return o;
}

const expected = fieldCount * (fieldCount - 1) / 2;
const object = make(-1);

for (let round = 0; round < 4; ++round) {
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(sum(object), expected);

    // Each field has its own exits, so these exits are spread over the whole function.
    // Each of them fires more than once.
    for (let k = 0; k < 3; ++k) {
        for (let index = 1; index < fieldCount; index += 6) {
            shouldBe(sum(make(index, 0.5)), expected - index + 0.5);
            shouldBe(sum(make(index, 2147483647)), expected - index + 2147483647);
        }
    }
}
