function shouldBe(actual, expected, search, start, end) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual} expected: ${expected} ${search} ${start} ${end}`);
}

var period = "abcde";
var base8 = period.repeat(20000);
base8.charCodeAt(0);
var base16 = period.repeat(20000) + "☃";
base16.charCodeAt(0);

function expectedStartsWith(start, end, search) {
    if (end - start < search.length)
        return false;
    for (var i = 0; i < search.length; ++i) {
        if (base8.charCodeAt(start + i) !== search.charCodeAt(i))
            return false;
    }
    return true;
}

function expectedEndsWith(start, end, search) {
    if (end - start < search.length)
        return false;
    return expectedStartsWith(end - search.length, end, search);
}

var cases = [];
for (var length of [1, 2, 3, 4, 6, 8, 11, 16]) {
    var search = base8.slice(2, 2 + length);
    for (var string of [search, "!" + search.slice(1), search.slice(0, -1) + "!"]) {
        cases.push({
            search: string,
            startsWith: new Function("s", `return s.startsWith("${string}");`),
            endsWith: new Function("s", `return s.endsWith("${string}");`),
        });
    }
}

for (var i = 0; i < testLoopCount; ++i) {
    var start = (i * 7919) % 99900;
    var end = start + 3 + (i * 3) % 19;
    for (var { search, startsWith, endsWith } of cases) {
        shouldBe(startsWith(base8.slice(start, end)), expectedStartsWith(start, end, search), search, start, end);
        shouldBe(endsWith(base8.slice(start, end)), expectedEndsWith(start, end, search), search, start, end);
    }

    var { search, startsWith, endsWith } = cases[i % cases.length];
    shouldBe(startsWith(base16.slice(start, end)), expectedStartsWith(start, end, search), search, start, end);
    shouldBe(endsWith(base16.slice(start, end)), expectedEndsWith(start, end, search), search, start, end);
    shouldBe(startsWith(base8.slice(start, start + 3) + base8.slice(start + 3, end + 1)), expectedStartsWith(start, end + 1, search), search, start, end);
    shouldBe(endsWith(base8.slice(start, start + 3) + base8.slice(start + 3, end + 1)), expectedEndsWith(start, end + 1, search), search, start, end);
}

(function () {
    var length = 3000;
    var wide = "☃" + "o".repeat(length);
    wide.charCodeAt(0);
    var key16 = wide.slice(1);
    var object = { };
    object[key16] = 1;
    var narrow = "o".repeat(length);
    narrow.charCodeAt(0);
    var slice = narrow.slice(1);
    function startsWith(s) { return s.startsWith("ooo"); }
    function endsWith(s) { return s.endsWith("ooo"); }
    noInline(startsWith);
    noInline(endsWith);
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(startsWith(narrow.slice(1 + (i % 7))), true);
        shouldBe(endsWith(narrow.slice(1 + (i % 7))), true);
    }
    object[narrow] = 2;
    shouldBe(startsWith(slice), true);
    shouldBe(endsWith(slice), true);
})();
