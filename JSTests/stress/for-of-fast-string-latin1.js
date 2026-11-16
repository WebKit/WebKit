function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

function codes(string) {
    let result = [];
    for (let character of string) {
        shouldBe(character.length, 1);
        result.push(character.charCodeAt(0));
    }
    return result;
}
noInline(codes);

function firstThree(string) {
    let [a, b, c] = string;
    return [a, b, c];
}
noInline(firstThree);

let resolved = "";
for (let i = 0; i < 256; i++)
    resolved += String.fromCharCode(i);
resolved.charCodeAt(0);

function check(string, offset, length) {
    let result = codes(string);
    shouldBe(result.length, length);
    for (let i = 0; i < length; i++)
        shouldBe(result[i], offset + i);
}

for (let i = 0; i < testLoopCount; i++) {
    let offset = i % 241;
    check(resolved, 0, 256);
    check(resolved.substring(offset, offset + 16), offset, 16);
    check(resolved.substring(offset, offset + 8) + resolved.substring(offset + 8, offset + 16), offset, 16);
    let [a, b, c] = firstThree("\xfd\xfe\xff");
    shouldBe(a, "\xfd");
    shouldBe(b, "\xfe");
    shouldBe(c, "\xff");
    [a, b, c] = firstThree("\xff");
    shouldBe(a, "\xff");
    shouldBe(b, undefined);
    shouldBe(c, undefined);
    shouldBe(codes(resolved.substring(offset, offset + 16) + "\u3042").length, 17);
    shouldBe(codes("").length, 0);
}
