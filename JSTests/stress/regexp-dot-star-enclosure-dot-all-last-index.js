function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

// Checks exec(), the lastIndex it leaves behind, and test(), which share the compiled path.
function check(re, lastIndex, string, expectedIndex, expectedMatch) {
    re.lastIndex = lastIndex;
    const result = re.exec(string);
    if (expectedMatch === null) {
        shouldBe(result, null);
        shouldBe(re.lastIndex, 0);
    } else {
        shouldBe(result[0], expectedMatch);
        shouldBe(result.index, expectedIndex);
        shouldBe(re.lastIndex, expectedIndex + expectedMatch.length);
    }

    re.lastIndex = lastIndex;
    shouldBe(re.test(string), expectedMatch !== null);
}

const dotAll = /.*X.*/gs;
const dotAllEOL = /.*X.*$/gs;
const dotAllMultiline = /.*X.*/gms;
// `s` is a no-op by construction here, since [\s\S] already matches every code point, so this must
// agree with the plain `g` spelling below.
const explicitAnyDotAll = new RegExp("[\\s\\S]*X[\\s\\S]*", "gs");
const plain = /.*X.*/g;

const bolDotAll = /^.*X.*/gs;
const bolEOLDotAll = /^.*X.*$/gs;
const bolDotAllMultiline = /^.*X.*/gms;
const bolEOLDotAllMultiline = /^.*X.*$/gms;
const bolPlain = /^.*X.*/g;
const bolPlainMultiline = /^.*X.*/gm;

function step() {
    // 1. A global match must begin at or after lastIndex.
    check(dotAll, 0, "aaXb", 0, "aaXb");
    check(dotAll, 1, "aaXb", 1, "aXb");
    check(dotAll, 2, "aaXb", 2, "Xb");
    check(dotAll, 3, "aaXb", 0, null);
    check(dotAll, 4, "aaXb", 0, null);

    check(dotAllEOL, 1, "aaXb", 1, "aXb");
    check(dotAll, 1, "aaXbXc", 1, "aXbXc");

    // The enclosure still reaches across line terminators; it just cannot reach past lastIndex.
    check(dotAll, 1, "aa\nXb", 1, "a\nXb");
    check(dotAllMultiline, 1, "aa\nXb", 1, "a\nXb");

    check(explicitAnyDotAll, 1, "aaXb", 1, "aXb");
    check(plain, 1, "aaXb", 1, "aXb");

    // matchAll() starts from lastIndex too, and its first yield was wrong for the same reason.
    dotAll.lastIndex = 0;
    const all = [...("aaXb".matchAll(dotAll))];
    shouldBe(all.length, 1);
    shouldBe(all[0].index, 0);
    shouldBe(all[0][0], "aaXb");

    // 2. `^` still has to hold where the match is reported to begin.
    check(bolDotAll, 0, "aaXb", 0, "aaXb");
    check(bolDotAll, 1, "aaXb", 0, null);
    check(bolDotAll, 2, "aaXb", 0, null);
    check(bolEOLDotAll, 0, "aaXb", 0, "aaXb");
    check(bolEOLDotAll, 1, "aaXb", 0, null);

    // Non-zero lastIndex is fine when `^` genuinely holds: under `m` it holds after a newline.
    check(bolDotAllMultiline, 0, "aa\nXb", 0, "aa\nXb");
    check(bolDotAllMultiline, 1, "aa\nXb", 3, "Xb");
    check(bolDotAllMultiline, 3, "aa\nXb", 3, "Xb");
    check(bolDotAllMultiline, 1, "aaXb", 0, null);
    check(bolEOLDotAllMultiline, 1, "aa\nXb", 3, "Xb");

    // Under `m` the match can even have to begin at a line start *after* the X the enclosure would
    // have matched: the leftmost X is at 1, but `^` does not hold there, so the match is "cXd" at 4.
    // Widening backwards cannot reach that, which is why these patterns skip the enclosure.
    check(bolDotAllMultiline, 1, "aXb\ncXd", 4, "cXd");
    check(bolDotAllMultiline, 2, "aXb\ncXd", 4, "cXd");
    check(bolDotAllMultiline, 4, "aXb\ncXd", 4, "cXd");
    check(bolDotAllMultiline, 5, "aXb\ncXd", 0, null);
    check(bolEOLDotAllMultiline, 1, "aXb\ncXd", 4, "cXd");

    // Non-dotAll spellings keep using the enclosure and must be unaffected.
    check(bolPlain, 0, "aaXb", 0, "aaXb");
    check(bolPlain, 1, "aaXb", 0, null);
    check(bolPlainMultiline, 0, "aa\nXb", 3, "Xb");
    check(bolPlainMultiline, 1, "aa\nXb", 3, "Xb");
}

for (var i = 0; i < testLoopCount; ++i)
    step();
