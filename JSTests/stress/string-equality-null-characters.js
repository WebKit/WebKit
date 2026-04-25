function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const null8bit1 = "a" + "\u0000" + "b";
    const null8bit2 = "a" + "\u0000" + "b";
    const null8bit3 = "a" + "\u0000" + "c";
    shouldBe(null8bit1 === null8bit2, true);
    shouldBe(null8bit1 === null8bit3, false);
    
    const null16bit1 = "あ" + "\u0000" + "い";
    const null16bit2 = "あ" + "\u0000" + "い";
    const null16bit3 = "あ" + "\u0000" + "う";
    shouldBe(null16bit1 === null16bit2, true);
    shouldBe(null16bit1 === null16bit3, false);
    
    const multiNull1 = "\u0000" + "\u0000" + "\u0000";
    const multiNull2 = "\u0000" + "\u0000" + "\u0000";
    const multiNull3 = "\u0000" + "\u0000";
    shouldBe(multiNull1 === multiNull2, true);
    shouldBe(multiNull1 === multiNull3, false);
    
    const control1 = "a" + "\u0001\u0002\u0003" + "b";
    const control2 = "a" + "\u0001\u0002\u0003" + "b";
    const control3 = "a" + "\u0001\u0002\u0004" + "b";
    shouldBe(control1 === control2, true);
    shouldBe(control1 === control3, false);
    
    const whitespace1 = "a" + "\t\n\r" + "b";
    const whitespace2 = "a" + "\t\n\r" + "b";
    const whitespace3 = "a" + "\t\n " + "b";
    shouldBe(whitespace1 === whitespace2, true);
    shouldBe(whitespace1 === whitespace3, false);
    
    const mixedNull1 = "テスト" + "\u0000" + "test";
    const mixedNull2 = "テスト" + "\u0000" + "test";
    const mixedNull3 = "テスト" + "\u0001" + "test";
    shouldBe(mixedNull1 === mixedNull2, true);
    shouldBe(mixedNull1 === mixedNull3, false);
    
    const startNull1 = "\u0000" + "hello";
    const startNull2 = "\u0000" + "hello";
    const endNull1 = "hello" + "\u0000";
    const endNull2 = "hello" + "\u0000";
    shouldBe(startNull1 === startNull2, true);
    shouldBe(endNull1 === endNull2, true);
    shouldBe(startNull1 === endNull1, false);
    
    const dynamic1 = "test\u0000" + i.toString();
    const dynamic2 = "test\u0000" + i.toString();
    shouldBe(dynamic1 === dynamic2, true);
}