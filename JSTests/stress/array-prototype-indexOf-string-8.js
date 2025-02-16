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
        "aasdfflkjlkj",
        "bdkjsldfk",
        "cadsflkj",
        "dasdlkjf",
        "afdutnklkj",
        "fadajknkj",
        "",
    ];

    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(test1(array, "aasdfflkjlkj"), 0);
        shouldBe(test1(array, ""), 6);
        shouldBe(test1(array, "aaasdfflkjlki"), -1);
        shouldBe(test1(array, "cadsflkj"), 2);
        shouldBe(test1(array, "afdutnklkj"), 4);
        shouldBe(test1(array, "e"), -1);
        shouldBe(test1(array, "あああああ"), -1);
        shouldBe(test2(array, "aaasdfflkjlkj", 1), -1);
        shouldBe(test2(array, "bdkjsldfk", 1), 1);
    }
}

{
    const bigArray = [];
    for (let i = 0; i < testLoopCount; i++) {
        bigArray.push(i.toString());
    }
    for (let i = 0; i < testLoopCount; i++) {
        let search = (i % 50).toString();
        let result = test1(bigArray, search);
        shouldBe(result.toString(), search);
    }
}
