//@ requireOptions("--jitPolicyScale=0.1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function opt(s, i) {
    let a = s.charCodeAt(i);
    let b = s.charCodeAt(i);
    return `${a}_${b}_end`;
}
noInline(opt);

for (let j = 0; j < 20000; j++)
    shouldBe(opt("hello", 1), "101_101_end");
for (let j = 0; j < 200; j++)
    shouldBe(opt("hello", 100), "NaN_NaN_end");
for (let j = 0; j < 20000; j++) {
    shouldBe(opt("hello", 1), "101_101_end");
    shouldBe(opt("hello", 100), "NaN_NaN_end");
}
