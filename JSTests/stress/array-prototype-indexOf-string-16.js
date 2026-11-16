function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test1(array, searchElement) {
    return array.indexOf(searchElement);
}
noInline(test1);

function test2(array, searchElement, fromIndex) {
    return array.indexOf(searchElement, fromIndex);
}
noInline(test2);

{
    const array = [
        "あいうえお",
        "かきくけここ",
        "さしすせ",
        "たちつてととと",
        "なにぬ",
        "まみむめもももも"
    ];

    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(test1(array, "あいうえお"), 0);
        shouldBe(test1(array, "あいうえおお"), -1);
        shouldBe(test1(array, "さしすせ"), 2);
        shouldBe(test1(array, "なにぬ"), 4);
        shouldBe(test1(array, "お"), -1);
        shouldBe(test1(array, "aaaaaaa"), -1);
        shouldBe(test2(array, "あいうえお", 1), -1);
        shouldBe(test2(array, "かきくけここ", 1), 1);
    }
}

{
    const array = [];
    for (let code = 0x4E00; code <= 0x9FFF; code++) {
      const char = String.fromCharCode(code);
      array.push(char);
    }

    for (let i = 0; i < testLoopCount; i++) {
        let code = 0x4E00 + (i % 50);
        let searchElement = String.fromCharCode(code);
        shouldBe(test1(array, searchElement), i % 50);
    }
}
