function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function charCodeAt(string, index) {
    return string.charCodeAt(index);
}
noInline(charCodeAt);

let string8 = "hello world!";
let string16 = "あいうえお";

for (let i = 0; i < testLoopCount; i++) {
    let index = i % 20;
    shouldBe(charCodeAt(string8, index), index < string8.length ? string8.charCodeAt(index) : NaN);
    index = i % 9;
    shouldBe(charCodeAt(string16, index), index < string16.length ? string16.charCodeAt(index) : NaN);
}

shouldBe(charCodeAt(string8, -1), NaN);
shouldBe(charCodeAt(string8, 4294967296), NaN);
shouldBe(charCodeAt(string8, 0.5), 104);
shouldBe(charCodeAt("", 0), NaN);
