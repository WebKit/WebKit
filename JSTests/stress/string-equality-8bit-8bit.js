function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const testLoopCount = 10000;

for (let i = 0; i < testLoopCount; i++) {
    const str1 = "hello" + " world";
    const str2 = "hello" + " world";
    const str3 = "hel" + "lo";
    const str4 = "hello" + " world!";
    
    shouldBe(str1 === str2, true);
    shouldBe(str1 == str2, true);
    
    shouldBe(str1 === str3, false);
    shouldBe(str1 == str3, false);
    
    shouldBe(str1 === str4, false);
    shouldBe(str1 == str4, false);
    
    shouldBe(str1 === str1, true);
    
    const newStr1 = "test" + i.toString();
    const newStr2 = "test" + i.toString();
    shouldBe(newStr1 === newStr2, true);
    
    const large1 = "a".repeat(1000);
    const large2 = "a".repeat(1000);
    const large3 = "a".repeat(999) + "b";
    shouldBe(large1 === large2, true);
    shouldBe(large1 === large3, false);
}