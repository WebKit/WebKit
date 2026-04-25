function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const str16bit = "こん" + "にちは";
    const str8bit = "hel" + "lo";
    
    shouldBe(str16bit === str8bit, false);
    shouldBe(str16bit == str8bit, false);
    
    const japanese = "１" + "２３";
    const ascii = "1" + "23";
    shouldBe(japanese === ascii, false);
    shouldBe(japanese == ascii, false);
    
    const fullwidthA = "Ａ";
    const asciiA = "A";
    shouldBe(fullwidthA === asciiA, false);
    shouldBe(fullwidthA == asciiA, false);
    
    const dynamic16bit = "テ" + "スト" + i.toString();
    const dynamic8bit = "te" + "st" + i.toString();
    shouldBe(dynamic16bit === dynamic8bit, false);
    
    const nonEmpty16bit = "あ";
    const empty8bit = "";
    shouldBe(nonEmpty16bit === empty8bit, false);
    
    const large16bit = "あ".repeat(1000);
    const large8bit = "a".repeat(1000);
    shouldBe(large16bit === large8bit, false);
    
    const symbols = "🌸" + "🗾🍣";
    const asciiSymbols = "a" + "bc";
    shouldBe(symbols === asciiSymbols, false);
}