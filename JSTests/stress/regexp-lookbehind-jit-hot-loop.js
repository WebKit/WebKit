function shouldBe(actual, expected) {
    actual = JSON.stringify(actual, (key, value) => value === undefined ? "<undefined>" : value);
    expected = JSON.stringify(expected, (key, value) => value === undefined ? "<undefined>" : value);
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function execConstant() {
    return /(?<=(a+))b(?<!c)/.exec("xaab");
}
noInline(execConstant);

function testVarying(string) {
    return /(?<! cu)bot|(?<=[/ ])curl/i.test(string);
}
noInline(testVarying);

function replaceAll(string) {
    return string.replace(/(?<=\d)(?=(?:\d{3})+$)/g, ",");
}
noInline(replaceAll);

function searchSticky(re, string, lastIndex) {
    re.lastIndex = lastIndex;
    return re.exec(string);
}
noInline(searchSticky);

let sticky = /(?<=a(b*))c/y;
let subjects = ["Googlebot", "a cubot", "get curl/8", "libcurl", "abbc"];
for (let i = 0; i < testLoopCount; ++i) {
    let match = execConstant();
    shouldBe([match.index, ...match], [3, "b", "aa"]);
    shouldBe(testVarying(subjects[i % subjects.length]), [true, false, true, false, false][i % subjects.length]);
    shouldBe(replaceAll("1234567"), "1,234,567");
    let stickyMatch = searchSticky(sticky, "xabbcabc", i % 2 ? 7 : 4);
    shouldBe([stickyMatch.index, ...stickyMatch], i % 2 ? [7, "c", "b"] : [4, "c", "bb"]);
    shouldBe(searchSticky(sticky, "xabbcabc", 5), null);
}
