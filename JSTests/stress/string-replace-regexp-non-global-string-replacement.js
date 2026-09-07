function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected));
}

function replaceEmpty(string, regExp) {
    return string.replace(regExp, "");
}
noInline(replaceEmpty);

function replaceString(string, regExp, replacement) {
    return string.replace(regExp, replacement);
}
noInline(replaceString);

const rope = "http://" + "example.com/" + "path".repeat(3);
const substring = "xxhttps://example.com/index.htmlyy".slice(2, -2);
const wide = "https://あいう.example/え";

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(replaceEmpty("https://example.com/", /^https?:\/\//), "example.com/");
    shouldBe(replaceEmpty("example.com/", /^https?:\/\//), "example.com/");
    shouldBe(replaceEmpty("http://", /^https?:\/\//), "");
    shouldBe(replaceEmpty("/a/b///", /\/+$/), "/a/b");
    shouldBe(replaceEmpty("file.tar.gz", /\.[^.]+$/), "file.tar");
    shouldBe(replaceEmpty("abc", /b/), "ac");
    shouldBe(replaceEmpty("ab", /b/), "a");
    shouldBe(replaceEmpty("ba", /b/), "a");
    shouldBe(replaceEmpty("abc", /(?:)/), "abc");
    shouldBe(replaceEmpty(rope, /^https?:\/\//), "example.com/pathpathpath");
    shouldBe(replaceEmpty(substring, /^https?:\/\//), "example.com/index.html");
    shouldBe(replaceEmpty(substring, /\.html$/), "https://example.com/index");
    shouldBe(replaceEmpty(wide, /^https?:\/\//), "あいう.example/え");
    shouldBe(replaceEmpty(wide, /\.example/), "https://あいう/え");

    shouldBe(replaceString("/api/v2/items", /\/v\d+\//, "/latest/"), "/api/latest/items");
    shouldBe(replaceString("v2/items", /^v\d+/, "latest"), "latest/items");
    shouldBe(replaceString("items/v2", /v\d+$/, "latest"), "items/latest");
    shouldBe(replaceString("v2", /v\d+/, "latest"), "latest");
    shouldBe(replaceString("abc", /(?:)/, "-"), "-abc");
    shouldBe(replaceString(rope, /example/, "あ"), "http://あ.com/pathpathpath");
    shouldBe(replaceString(wide, /い/, "i"), "https://あiう.example/え");
    shouldBe(replaceString("a-b", /(\w)-(\w)/, "$2-$1"), "b-a");
    shouldBe(replaceString("a-b", /-/, "[$&]"), "a[-]b");
}

shouldBe(replaceString("prefix:value", /:/, "="), "prefix=value");
shouldBe(RegExp.leftContext, "prefix");
shouldBe(RegExp.rightContext, "value");
shouldBe(RegExp.lastMatch, ":");

const result = replaceEmpty("https://example.com/", /^https?:\/\//);
shouldBe(result.length, 12);
shouldBe(result.charCodeAt(0), "e".charCodeAt(0));
shouldBe(result + "!", "example.com/!");
const map = new Map([[result, 1]]);
shouldBe(map.get("example.com/"), 1);
