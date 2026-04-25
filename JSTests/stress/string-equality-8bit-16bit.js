function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const str8bit = "hel" + "lo";
    const str16bit = "こん" + "にちは";
    
    shouldBe(str8bit === str16bit, false);
    shouldBe(str8bit == str16bit, false);
    
    const ascii = "1" + "23";
    const japanese = "１" + "２３";
    shouldBe(ascii === japanese, false);
    shouldBe(ascii == japanese, false);
    
    const asciiA = "A";
    const fullwidthA = "Ａ";
    shouldBe(asciiA === fullwidthA, false);
    shouldBe(asciiA == fullwidthA, false);
    
    const dynamic8bit = "te" + "st" + i.toString();
    const dynamic16bit = "テ" + "スト" + i.toString();
    shouldBe(dynamic8bit === dynamic16bit, false);
    
    const empty8bit = "";
    const nonEmpty16bit = "あ";
    shouldBe(empty8bit === nonEmpty16bit, false);
    
    const large8bit = "a".repeat(1000);
    const large16bit = "あ".repeat(1000);
    shouldBe(large8bit === large16bit, false);
}