function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const math1 = "𝔸" + "𝔹ℂ𝔻";
    const math2 = "𝔸" + "𝔹ℂ𝔻";
    const math3 = "𝔸" + "𝔹ℂ𝔼";
    shouldBe(math1 === math2, true);
    shouldBe(math1 === math3, false);
    
    const ancient1 = "𐊀" + "𐊁𐊂";
    const ancient2 = "𐊀" + "𐊁𐊂";
    const ancient3 = "𐊀" + "𐊁𐊃";
    shouldBe(ancient1 === ancient2, true);
    shouldBe(ancient1 === ancient3, false);
    
    const music1 = "𝄞" + "𝄢𝄫";
    const music2 = "𝄞" + "𝄢𝄫";
    const music3 = "𝄞" + "𝄢𝄪";
    shouldBe(music1 === music2, true);
    shouldBe(music1 === music3, false);
    
    const high1 = String.fromCharCode(0xD800);
    const low1 = String.fromCharCode(0xDC00);
    const surrogate1 = high1 + low1;
    const surrogate2 = high1 + low1;
    const low2 = String.fromCharCode(0xDC01);
    const surrogate3 = high1 + low2;
    shouldBe(surrogate1 === surrogate2, true);
    shouldBe(surrogate1 === surrogate3, false);
    
    const mixed1 = "A" + "𝔸B";
    const mixed2 = "A" + "𝔸B";
    const mixed3 = "A" + "𝔹B";
    shouldBe(mixed1 === mixed2, true);
    shouldBe(mixed1 === mixed3, false);
    
    const large1 = "𝔸".repeat(100);
    const large2 = "𝔸".repeat(100);
    const large3 = "𝔸".repeat(99) + "𝔹";
    shouldBe(large1 === large2, true);
    shouldBe(large1 === large3, false);
    
    const baseChar = "𝕏";
    const dynamic1 = baseChar + i.toString();
    const dynamic2 = baseChar + i.toString();
    shouldBe(dynamic1 === dynamic2, true);
}