function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const str1 = "こん" + "にちは" + "世界";
    const str2 = "こん" + "にちは" + "世界";
    const str3 = "こん" + "にちは";
    const str4 = "こん" + "にちは" + "世界!";
    
    shouldBe(str1 === str2, true);
    shouldBe(str1 == str2, true);
    
    shouldBe(str1 === str3, false);
    shouldBe(str1 == str3, false);
    
    shouldBe(str1 === str4, false);
    shouldBe(str1 == str4, false);
    
    shouldBe(str1 === str1, true);
    
    const newStr1 = "テスト" + i.toString();
    const newStr2 = "テスト" + i.toString();
    shouldBe(newStr1 === newStr2, true);
    
    const large1 = "あ".repeat(1000);
    const large2 = "あ".repeat(1000);
    const large3 = "あ".repeat(999) + "い";
    shouldBe(large1 === large2, true);
    shouldBe(large1 === large3, false);
    
    const mixed1 = "ひらがな" + "カタカナ" + "漢字";
    const mixed2 = "ひらがな" + "カタカナ" + "漢字";
    const mixed3 = "ひらがな" + "カタカナ" + "感じ";
    shouldBe(mixed1 === mixed2, true);
    shouldBe(mixed1 === mixed3, false);
}