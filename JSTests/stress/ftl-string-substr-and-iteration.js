function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function substr(string, start, length) { return string.substr(start, length); }
noInline(substr);

function concatenateCharacters(string) {
    let result = "";
    for (let character of string)
        result += character;
    return result;
}
noInline(concatenateCharacters);

let strings = ["abcdef", "あいうえお", "a\u{1F600}b", ""];

for (let i = 0; i < testLoopCount; ++i) {
    let string = strings[i % strings.length];
    let start = i % 8 - 2;
    let length = i % 5 - 1;
    let expected = "";
    let from = start < 0 ? Math.max(0, string.length + start) : Math.min(start, string.length);
    for (let j = from; j < Math.min(string.length, from + length); ++j)
        expected += string[j];
    shouldBe(substr(string, start, length), expected, `${escape(string)}.substr(${start}, ${length})`);
    shouldBe(concatenateCharacters(string), string, `iteration over ${escape(string)}`);
}
