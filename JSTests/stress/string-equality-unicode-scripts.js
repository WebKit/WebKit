function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const arabic1 = "السلام" + " عليكم";
    const arabic2 = "السلام" + " عليكم";
    const arabic3 = "السلام" + " عليكن";
    shouldBe(arabic1 === arabic2, true);
    shouldBe(arabic1 === arabic3, false);
    
    const cyrillic1 = "Привет" + " мир";
    const cyrillic2 = "Привет" + " мир";
    const cyrillic3 = "Привет" + " вам";
    shouldBe(cyrillic1 === cyrillic2, true);
    shouldBe(cyrillic1 === cyrillic3, false);
    
    const greek1 = "Γεια σας" + " κόσμος";
    const greek2 = "Γεια σας" + " κόσμος";
    const greek3 = "Γεια σου" + " κόσμος";
    shouldBe(greek1 === greek2, true);
    shouldBe(greek1 === greek3, false);
    
    const hebrew1 = "שלום" + " עולם";
    const hebrew2 = "שלום" + " עולם";
    const hebrew3 = "שלום" + " לכם";
    shouldBe(hebrew1 === hebrew2, true);
    shouldBe(hebrew1 === hebrew3, false);
    
    const chinese1 = "你好" + "世界";
    const chinese2 = "你好" + "世界";
    const chinese3 = "你好" + "朋友";
    shouldBe(chinese1 === chinese2, true);
    shouldBe(chinese1 === chinese3, false);
    
    const korean1 = "안녕" + "하세요";
    const korean2 = "안녕" + "하세요";
    const korean3 = "안녕히" + "가세요";
    shouldBe(korean1 === korean2, true);
    shouldBe(korean1 === korean3, false);
    
    const thai1 = "สวัสดี" + "ครับ";
    const thai2 = "สวัสดี" + "ครับ";
    const thai3 = "สวัสดี" + "ค่ะ";
    shouldBe(thai1 === thai2, true);
    shouldBe(thai1 === thai3, false);
    
    const mixed1 = "Hello" + "世界" + "Мир";
    const mixed2 = "Hello" + "世界" + "Мир";
    const mixed3 = "Hello" + "世界" + "Мира";
    shouldBe(mixed1 === mixed2, true);
    shouldBe(mixed1 === mixed3, false);
    
    const scripts = ["α", "β", "γ", "δ", "ε"];
    const script = scripts[i % scripts.length];
    const dynamic1 = "test" + script;
    const dynamic2 = "test" + script;
    shouldBe(dynamic1 === dynamic2, true);
}