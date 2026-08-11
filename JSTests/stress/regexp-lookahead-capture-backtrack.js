function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + " expected: " + expected + (msg ? " (" + msg + ")" : ""));
}

function shouldBeNull(actual, msg) {
    if (actual !== null)
        throw new Error("Bad value: " + JSON.stringify(actual) + " expected: null" + (msg ? " (" + msg + ")" : ""));
}

function shouldBeUndefined(actual, msg) {
    if (actual !== undefined)
        throw new Error("Bad value: " + JSON.stringify(actual) + " expected: undefined" + (msg ? " (" + msg + ")" : ""));
}

for (var i = 0; i < testLoopCount; i++) {
    var r;

    r = "x".match(/(?:(?=(x))y|x)/);
    shouldBe(r[0], "x", "case 1 match");
    shouldBeUndefined(r[1], "case 1 group 1");

    r = "ab".match(/(?:(?=(a)(b))x|.)/);
    shouldBe(r[0], "a", "case 2 match");
    shouldBeUndefined(r[1], "case 2 group 1");
    shouldBeUndefined(r[2], "case 2 group 2");

    r = "abcdef".match(/(?:c(?=(def))x|c)/);
    shouldBe(r[0], "c", "case 3 match");
    shouldBeUndefined(r[1], "case 3 group 1");

    r = "abc".match(/(?:(?=(abc))d|a)/);
    shouldBe(r[0], "a", "case 4 match");
    shouldBeUndefined(r[1], "case 4 group 1");

    r = "ab".match(/(?:(?:(?=(ab))c|a)d|ab)/);
    shouldBe(r[0], "ab", "case 5 match");
    shouldBeUndefined(r[1], "case 5 group 1");

    r = "x".match(/(?:(?=(x))(?=(x))y|x)/);
    shouldBe(r[0], "x", "case 6 match");
    shouldBeUndefined(r[1], "case 6 group 1");
    shouldBeUndefined(r[2], "case 6 group 2");

    r = "x".match(/(?:y|(?=(x))z|x)/);
    shouldBe(r[0], "x", "case 7 match");
    shouldBeUndefined(r[1], "case 7 group 1");

    r = "ab".match(/(?:(?=(a)(b))xy|ab)/);
    shouldBe(r[0], "ab", "case 8 match");
    shouldBeUndefined(r[1], "case 8 group 1");
    shouldBeUndefined(r[2], "case 8 group 2");

    r = "a".match(/(?:(?=(a))b|(?=(c))d|a)/);
    shouldBe(r[0], "a", "case 9 match");
    shouldBeUndefined(r[1], "case 9 group 1");
    shouldBeUndefined(r[2], "case 9 group 2");

    r = "x".match(/(?:(?=(?<n>x))y|x)/);
    shouldBe(r[0], "x", "case 10 match");
    shouldBeUndefined(r[1], "case 10 named group");

    r = "abc".match(/(?:(?=(abc))+d|abc)/);
    shouldBe(r[0], "abc", "case 11 match");
    shouldBeUndefined(r[1], "case 11 group 1");

    r = "a".match(/(?:(?=(a))b|a)(.?)/);
    shouldBe(r[0], "a", "case 12 match");
    shouldBeUndefined(r[1], "case 12 group 1");
    shouldBe(r[2], "", "case 12 group 2");

    r = "x".match(/(?:(?=(?<a>x))y|(?=(?<a>x))x)/);
    shouldBe(r[0], "x", "case 13 match");
    shouldBe(r.groups.a, "x", "case 13 duplicate named group");
}
