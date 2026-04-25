function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const hiragana1 = "あい" + "うえお";
    const hiragana2 = "あい" + "うえお";
    const hiragana3 = "あい" + "うえか";
    shouldBe(hiragana1 === hiragana2, true);
    shouldBe(hiragana1 === hiragana3, false);
    
    const katakana1 = "アイ" + "ウエオ";
    const katakana2 = "アイ" + "ウエオ";
    const katakana3 = "アイ" + "ウエカ";
    shouldBe(katakana1 === katakana2, true);
    shouldBe(katakana1 === katakana3, false);
    
    const kanji1 = "日本" + "語文字";
    const kanji2 = "日本" + "語文字";
    const kanji3 = "日本" + "語文章";
    shouldBe(kanji1 === kanji2, true);
    shouldBe(kanji1 === kanji3, false);
    
    const mixed1 = "ひらがな" + "カタカナ" + "漢字";
    const mixed2 = "ひらがな" + "カタカナ" + "漢字";
    const mixed3 = "ひらがな" + "カタカナ" + "感じ";
    shouldBe(mixed1 === mixed2, true);
    shouldBe(mixed1 === mixed3, false);
    
    const sentence1 = "今日は" + "とても良い" + "天気ですね。";
    const sentence2 = "今日は" + "とても良い" + "天気ですね。";
    const sentence3 = "今日は" + "とても悪い" + "天気ですね。";
    shouldBe(sentence1 === sentence2, true);
    shouldBe(sentence1 === sentence3, false);
    
    const jpNum1 = "一二" + "三四五";
    const jpNum2 = "一二" + "三四五";
    const jpNum3 = "一二" + "三四六";
    shouldBe(jpNum1 === jpNum2, true);
    shouldBe(jpNum1 === jpNum3, false);
    
    const counter = String.fromCharCode(12354 + (i % 83));
    const dynamic1 = "テ" + "スト" + counter;
    const dynamic2 = "テ" + "スト" + counter;
    shouldBe(dynamic1 === dynamic2, true);
}