function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const mixed1 = "Hel" + "lo世界";
    const mixed2 = "Hel" + "lo世界";
    const mixed3 = "Hel" + "lo世";
    shouldBe(mixed1 === mixed2, true);
    shouldBe(mixed1 === mixed3, false);
    
    const num1 = "12" + "3あ";
    const num2 = "12" + "3あ";
    const num3 = "12" + "3い";
    shouldBe(num1 === num2, true);
    shouldBe(num1 === num3, false);
    
    const punct1 = "Hel" + "lo, 世界!";
    const punct2 = "Hel" + "lo, 世界!";
    const punct3 = "Hel" + "lo, 世界?";
    shouldBe(punct1 === punct2, true);
    shouldBe(punct1 === punct3, false);
    
    const space1 = "あ " + "い う";
    const space2 = "あ " + "い う";
    const space3 = "あ　" + "い　う";
    shouldBe(space1 === space2, true);
    shouldBe(space1 === space3, false);
    
    const dynamic1 = "te" + "st" + "テ" + "スト" + i;
    const dynamic2 = "te" + "st" + "テ" + "スト" + i;
    shouldBe(dynamic1 === dynamic2, true);
}