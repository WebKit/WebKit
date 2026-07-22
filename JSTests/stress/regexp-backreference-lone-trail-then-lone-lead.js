function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`FAIL ${message}: expected ${expected}, got ${actual}`);
}

const trail = "\uDC00";
const lead = "\uD800";
const pair = "\uD800\uDC00";

const units = [
    [trail + lead + "a", "trail lead a"],
    ["a" + trail + lead, "a trail lead"],
    [lead + "a" + trail, "lead a trail"],
    [trail + trail + "a", "trail trail a"],
    [lead + lead + "a", "lead lead a"],
];

for (const [unit, name] of units) {
    shouldBe([...unit].length, 3, `${name} code point count`);
    shouldBe([...(unit + unit)].length, 6, `${name} doubled code point count`);
}

for (let i = 0; i < testLoopCount; ++i) {
    for (const [unit, name] of units) {
        const doubled = unit + unit;
        shouldBe(/(?<=^)(...)\1/u.test(doubled), true, `${name} interpreter forward`);
        shouldBe(/^(...)\1/u.test(doubled), true, `${name} jit forward`);
        shouldBe(/(?<=^)(...)\1/iu.test(doubled), true, `${name} interpreter ignore case`);
        shouldBe(/(?<=(...)\1)x/u.test(doubled + "x"), true, `${name} interpreter backward`);
        shouldBe(/(?<=^)(...)\1/u.exec(doubled)[1], unit, `${name} interpreter capture`);
        shouldBe(/(?<=^)(...)\1/u.test(unit + trail + lead + "b"), false, `${name} interpreter mismatch`);
    }

    shouldBe(/(?<=^)(.)\1/u.test(pair + pair), true, "surrogate pair backreference");
    shouldBe(/(?<=^)(..)\1/u.test(pair + "a" + pair + "a"), true, "surrogate pair with trailing ascii");
    shouldBe(/(?<=^)(.)\1/u.test("\uD801\uD802"), false, "two lone leads are different code points");
    shouldBe(/(?<=^)(.)\1/u.test("\uDC01\uDC02"), false, "two lone trails are different code points");
    shouldBe(/(?<=^)(.)\1/u.test(pair + lead), false, "pair followed by lone lead");
}
