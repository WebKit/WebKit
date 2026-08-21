//@ skip if not $jitTests
//@ runDefault

// The interpreter reads several unicode lookbehinds incorrectly (bug 317275), so this only runs on the JIT.

function shouldBe(actual, expected) {
    actual = JSON.stringify(actual, (key, value) => value === undefined ? "<undefined>" : value);
    expected = JSON.stringify(expected, (key, value) => value === undefined ? "<undefined>" : value);
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function matchOf(re, string) {
    let match = re.exec(string);
    return match ? [match.index, ...match] : null;
}

function indicesOf(re, string) {
    let match = re.exec(string);
    return match ? [match.index, ...match, ...match.indices] : null;
}

function matchAllOf(re, string) {
    return [...string.matchAll(re)].map((match) => [match.index, ...match]);
}

function execAt(re, string, lastIndex) {
    re.lastIndex = lastIndex;
    let match = re.exec(string);
    return [match ? [match.index, ...match] : null, re.lastIndex];
}

shouldBe(matchOf(/(?<=\1(a))b/u, "aab"), [2, "b", "a"]);
shouldBe(matchOf(/(?<=\1(a))b/u, "xab"), null);
shouldBe(matchOf(/(?<=\1(a))b/v, "aab"), [2, "b", "a"]);
shouldBe(matchOf(/(?<=\1(\u{1F600}))b/u, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(\u{1F600}))b/u, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(\u{1F600}))b/u, "\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1(\u{1F600}))b/u, "a\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1(\u{1F600}))b/u, "\ude00\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1(\u{1F600}a))b/u, "\u{1f600}a\u{1f600}ab"), [6, "b", "\u{1f600}a"]);
shouldBe(matchOf(/(?<=\1(\u{1F600}a))b/u, "\u{1f600}b\u{1f600}ab"), null);
shouldBe(matchOf(/(?<=\1(a\u{1F600}))b/u, "a\u{1f600}a\u{1f600}b"), [6, "b", "a\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(a\u{1F600}))b/u, "a\u{1f601}a\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(.))b/su, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "\u{1f600}\ude00b"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "\ud83d\ud83db"), [2, "b", "\ud83d"]);
shouldBe(matchOf(/(?<=\1(.))b/su, "\ude00\ude00b"), [2, "b", "\ude00"]);
shouldBe(matchOf(/(?<=\1(.))b/su, "\ud83d\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "aab"), [2, "b", "a"]);
shouldBe(matchOf(/(?<=\1(.))b/su, "a\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "\u{1f600}ab"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "\ude00b"), null);
shouldBe(matchOf(/(?<=\1(.))b/su, "b"), null);
shouldBe(matchOf(/(?<=\1(.))b/sv, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(.))b/sv, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(.+))b/su, "\u{1f600}a\u{1f600}ab"), [6, "b", "\u{1f600}a"]);
shouldBe(matchOf(/(?<=\1(.+))b/su, "a\u{1f600}a\u{1f600}b"), [6, "b", "a\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(.+))b/su, "a\u{1f600}a\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(.+))b/su, "\u{1f600}\u{1f600}\u{1f600}\u{1f600}b"), [8, "b", "\u{1f600}\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(.+))b/su, "\u{1f600}\u{1f600}\u{1f600}b"), [6, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(.+?))b/su, "\u{1f600}\u{1f600}\u{1f600}\u{1f600}b"), [8, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(.+?))b/su, "\u{1f600}\u{1f601}\u{1f600}\u{1f601}b"), [8, "b", "\u{1f600}\u{1f601}"]);
shouldBe(matchOf(/(?<=\1(.{2}))b/su, "\u{1f600}a\u{1f600}ab"), [6, "b", "\u{1f600}a"]);
shouldBe(matchOf(/(?<=\1(.{2}))b/su, "\u{1f600}a\u{1f600}bb"), null);
shouldBe(matchOf(/(?<=\1(.{2}))b/su, "\u{1f600}\u{1f601}\u{1f600}\u{1f601}b"), [8, "b", "\u{1f600}\u{1f601}"]);
shouldBe(matchOf(/(?<=\1(.{2}))b/su, "\u{1f600}\u{1f601}\u{1f601}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(\p{Emoji_Presentation}))b/u, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(\p{Emoji_Presentation}))b/u, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1([\u{1F600}-\u{1F64F}]))b/u, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1([\u{1F600}-\u{1F64F}]))b/u, "\u{1f601}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1([\u{1F600}-\u{1F64F}]+))b/u, "\u{1f600}\u{1f601}\u{1f600}\u{1f601}b"), [8, "b", "\u{1f600}\u{1f601}"]);
shouldBe(matchOf(/(?<=\1([\u{1F600}-\u{1F64F}]+))b/u, "\u{1f600}\u{1f601}\u{1f601}\u{1f601}b"), [8, "b", "\u{1f601}"]);
shouldBe(matchOf(/(?<=\1([\u{1F600}-\u{1F64F}]+))b/u, "\u{1f600}\u{1f601}\u{1f601}\u{1f601}\u{1f601}b"), [10, "b", "\u{1f601}\u{1f601}"]);
shouldBe(matchOf(/(?<=\1{2}(\u{1F600}))b/u, "\u{1f600}\u{1f600}\u{1f600}b"), [6, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1{2}(\u{1F600}))b/u, "\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1{2}(\u{1F600}))b/u, "a\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1{2}(.))b/su, "\u{1f600}\u{1f600}\u{1f600}b"), [6, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1{2}(.))b/su, "\u{1f600}\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1{2}(.))b/su, "a\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\1{2}(.))b/su, "\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1*(\u{1F600}))b/u, "x\u{1f600}\u{1f600}\u{1f600}b"), [7, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1*(\u{1F600}))b/u, "x\u{1f600}b"), [3, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1*(\u{1F600}))b/u, "y\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1*(\u{1F600}))b/u, "x\u{1f601}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1*(\u{1F600}))b/u, "xa\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1*?(\u{1F600}))b/u, "x\u{1f600}\u{1f600}\u{1f600}b"), [7, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1*?(\u{1F600}))b/u, "y\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1?(\u{1F600}))b/u, "x\u{1f600}\u{1f600}b"), [5, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1?(\u{1F600}))b/u, "x\u{1f600}b"), [3, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1?(\u{1F600}))b/u, "x\u{1f600}\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1{0,2}(.))b/su, "x\u{1f600}\u{1f600}\u{1f600}b"), [7, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1{0,2}(.))b/su, "x\u{1f600}\u{1f600}\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1{0,2}(.))b/su, "x\u{1f600}b"), [3, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1{0,2}(.))b/su, "x\u{1f601}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=x\1{0,2}?(.))b/su, "x\u{1f600}\u{1f600}\u{1f600}b"), [7, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=x\1{0,2}?(.))b/su, "x\u{1f600}\u{1f600}\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(\u{1F600})b(?<=\1b)/u, "\u{1f600}b"), [0, "\u{1f600}b", "\u{1f600}"]);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1+b)/u, "x\u{1f600}\u{1f600}b"), [3, "\u{1f600}b", "\u{1f600}"]);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1+b)/u, "y\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1+b)/u, "x\u{1f601}\u{1f600}b"), null);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1+?b)/u, "x\u{1f600}\u{1f600}b"), [3, "\u{1f600}b", "\u{1f600}"]);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1{2}b)/u, "x\u{1f600}\u{1f600}b"), [3, "\u{1f600}b", "\u{1f600}"]);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1{2}b)/u, "x\u{1f600}b"), null);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1{2}b)/u, "x\u{1f600}\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1{1,2}b)/u, "x\u{1f600}\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1{1,2}?b)/u, "x\u{1f600}\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(\u{1F600})b(?<=x\1{1,2}?b)/u, "x\u{1f600}b"), [1, "\u{1f600}b", "\u{1f600}"]);
shouldBe(matchOf(/(.)b(?<=x\1+b)/su, "x\u{1f600}\u{1f600}b"), [3, "\u{1f600}b", "\u{1f600}"]);
shouldBe(matchOf(/(.)b(?<=x\1+b)/su, "x\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(.)b(?<=x\1+b)/su, "xaab"), [2, "ab", "a"]);
shouldBe(matchOf(/(.+)b(?<=x\1\1b)/su, "x\u{1f600}a\u{1f600}ab"), [4, "\u{1f600}ab", "\u{1f600}a"]);
shouldBe(matchOf(/(.+)b(?<=x\1\1b)/su, "x\u{1f600}a\u{1f600}bb"), null);
shouldBe(matchOf(/(.+)b(?<=x\1\1b)/su, "xa\u{1f600}a\u{1f600}b"), [4, "a\u{1f600}b", "a\u{1f600}"]);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{1F600}))b/u, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{1F600}))b/u, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{1F600})|\k<n>(?<n>\u{1F601}))b/u, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}", undefined]);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{1F600})|\k<n>(?<n>\u{1F601}))b/u, "\u{1f601}\u{1f601}b"), [4, "b", undefined, "\u{1f601}"]);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{1F600})|\k<n>(?<n>\u{1F601}))b/u, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{1F600})|\k<n>(?<n>\u{1F601}))b/u, "\u{1f601}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=\k<n>(?<n>.)|\k<n>(?<n>a))b/su, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}", undefined]);
shouldBe(matchOf(/(?<=\k<n>(?<n>.)|\k<n>(?<n>a))b/su, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\k<n>(?<n>.)|\k<n>(?<n>a))b/su, "aab"), [2, "b", "a", undefined]);
shouldBe(matchOf(/(?<=x\k<n>*(?<n>\u{1F600})|x\k<n>*(?<n>\u{1F601}))b/u, "x\u{1f601}\u{1f601}\u{1f601}b"), [7, "b", undefined, "\u{1f601}"]);
shouldBe(matchOf(/(?<=x\k<n>*(?<n>\u{1F600})|x\k<n>*(?<n>\u{1F601}))b/u, "x\u{1f600}\u{1f601}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=x\k<n>*(?<n>\u{1F600})|x\k<n>*(?<n>\u{1F601}))b/u, "y\u{1f601}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(a))b/iu, "aAb"), [2, "b", "A"]);
shouldBe(matchOf(/(?<=\1(a))b/iu, "Aab\u{1f600}"), [2, "b", "a"]);
shouldBe(matchOf(/(?<=\1(a))b/iu, "xAb\u{1f600}"), null);
shouldBe(matchOf(/(?<=\1(\u{1F600}))b/iu, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(\u{1F600}))b/iu, "\u{1f600}\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(\u{10400}))b/iu, "\u{10400}\u{10428}b"), [4, "b", "\u{10428}"]);
shouldBe(matchOf(/(?<=\1(\u{10400}))b/iu, "\u{10428}\u{10400}b"), [4, "b", "\u{10400}"]);
shouldBe(matchOf(/(?<=\1(\u{10400}))b/iu, "\u{10428}\u{10428}b"), [4, "b", "\u{10428}"]);
shouldBe(matchOf(/(?<=\1(\u{10400}))b/iu, "\u{10429}\u{10400}b"), null);
shouldBe(matchOf(/(?<=\1(\u{10400}))b/u, "\u{10428}\u{10400}b"), null);
shouldBe(matchOf(/(?<=\1(\u{10400}))b/iv, "\u{10428}\u{10400}b"), [4, "b", "\u{10400}"]);
shouldBe(matchOf(/(?<=\1(.))b/isu, "\u{10400}\u{10428}b"), [4, "b", "\u{10428}"]);
shouldBe(matchOf(/(?<=\1(.))b/isu, "\u{10429}\u{10428}b"), null);
shouldBe(matchOf(/(?<=\1(.+))b/isu, "\u{10400}a\u{10428}Ab"), [6, "b", "\u{10428}A"]);
shouldBe(matchOf(/(?<=\1(.+))b/isu, "\u{10400}a\u{10429}Ab"), null);
shouldBe(matchOf(/(?<=\1(k))x/iu, "k\u212ax"), [2, "x", "\u212a"]);
shouldBe(matchOf(/(?<=\1(k))x/iu, "\u212akx"), [2, "x", "k"]);
shouldBe(matchOf(/(?<=\1(k))x/iu, "\u212a\u212ax"), [2, "x", "\u212a"]);
shouldBe(matchOf(/(?<=\1(k))x/iu, "K\u212ax\u{1f600}"), [2, "x", "\u212a"]);
shouldBe(matchOf(/(?<=\1(s))x/iu, "s\u017fx"), [2, "x", "\u017f"]);
shouldBe(matchOf(/(?<=\1(σ))x/iu, "\u03c3\u03c2x"), [2, "x", "\u03c2"]);
shouldBe(matchOf(/(?<=\1(σ))x/iu, "\u03a3\u03c2x\u{1f600}"), [2, "x", "\u03c2"]);
shouldBe(matchOf(/(?<=\1{2}(\u{10400}))b/iu, "\u{10400}\u{10428}\u{10400}b"), [6, "b", "\u{10400}"]);
shouldBe(matchOf(/(?<=\1{2}(\u{10400}))b/iu, "\u{10400}\u{10428}\u{10401}b"), null);
shouldBe(matchOf(/(?<=x\1*(\u{10400}))b/iu, "x\u{10428}\u{10400}\u{10428}b"), [7, "b", "\u{10428}"]);
shouldBe(matchOf(/(?<=x\1*(\u{10400}))b/iu, "y\u{10428}\u{10400}\u{10428}b"), null);
shouldBe(matchOf(/(?<=x\1*?(\u{10400}))b/iu, "x\u{10428}\u{10400}\u{10428}b"), [7, "b", "\u{10428}"]);
shouldBe(matchOf(/(\u{10400})b(?<=x\1+b)/iu, "x\u{10428}\u{10400}\u{10428}b"), [5, "\u{10428}b", "\u{10428}"]);
shouldBe(matchOf(/(\u{10400})b(?<=x\1+b)/iu, "x\u{10428}\u{10401}\u{10428}b"), null);
shouldBe(matchOf(/(\u{10400})b(?<=x\1{2}b)/iu, "x\u{10428}\u{10400}b"), [3, "\u{10400}b", "\u{10400}"]);
shouldBe(matchOf(/(\u{10400})b(?<=x\1{1,2}?b)/iu, "x\u{10428}\u{10428}\u{10400}b"), null);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{10400})|\k<n>(?<n>\u{10401}))b/iu, "\u{10428}\u{10400}b"), [4, "b", "\u{10400}", undefined]);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{10400})|\k<n>(?<n>\u{10401}))b/iu, "\u{10429}\u{10401}b"), [4, "b", undefined, "\u{10401}"]);
shouldBe(matchOf(/(?<=\k<n>(?<n>\u{10400})|\k<n>(?<n>\u{10401}))b/iu, "\u{10429}\u{10400}b"), null);
shouldBe(matchOf(/(?<!\1(\u{1F600}))b/u, "\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<!\1(\u{1F600}))b/u, "\u{1f600}\u{1f601}b"), [4, "b", undefined]);
shouldBe(matchOf(/(?<!\1(\u{1F600}))b/u, "\u{1f600}b"), [2, "b", undefined]);
shouldBe(matchOf(/(?<!\1(.))b/su, "\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<!\1(.))b/su, "\u{1f600}\u{1f601}b"), [4, "b", undefined]);
shouldBe(matchOf(/(?<!\1(.))b/su, "a\u{1f600}b"), [3, "b", undefined]);
shouldBe(matchOf(/(?<=(\u{1F600})\1)b/u, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=(\u{1F600})\1)b/u, "\u{1f600}b"), [2, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=(\u{1F600})\1)b/u, "\u{1f601}b"), null);
shouldBe(matchOf(/(?<=(x\1)(\u{1F600}))b/u, "x\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=(x\1)(\u{1F600}))b/u, "\u{1f600}\u{1f600}b"), null);
shouldBe(matchOf(/(?<=(x\1)(\u{1F600}))b/u, "x\u{1f600}b"), [3, "b", "x", "\u{1f600}"]);
shouldBe(matchOf(/(?<=(x\1)?(\u{1F600}))b/u, "x\u{1f600}\u{1f600}b"), [5, "b", undefined, "\u{1f600}"]);
shouldBe(matchOf(/(?<=(x\1)?(\u{1F600}))b/u, "\u{1f600}b"), [2, "b", undefined, "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(?<=(\u{1F600})))b/u, "\u{1f600}b"), [2, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(?<=(\u{1F600})))b/u, "\u{1f601}b"), null);
shouldBe(matchOf(/(?<=\1(?<=(\u{1F600})))b/u, "ab"), null);
shouldBe(matchOf(/(?<=(?<=\1(\u{1F600}))a)b/u, "\u{1f600}\u{1f600}ab"), [5, "b", "\u{1f600}"]);
shouldBe(matchOf(/(?<=(?<=\1(\u{1F600}))a)b/u, "\u{1f600}\u{1f601}ab"), null);
shouldBe(/(?<=\1(\u{1F600}))b/u.test("\u{1f600}\u{1f600}b"), true);
shouldBe(/(?<=\1(\u{1F600}))b/u.test("\u{1f600}\u{1f601}b"), false);
shouldBe(/(?<=\1(.))b/su.test("\u{1f600}\u{1f600}b"), true);
shouldBe(/(?<=\1(.))b/su.test("\u{1f600}\u{1f601}b"), false);
shouldBe(/(?<=\1(\u{10400}))b/iu.test("\u{10428}\u{10400}b"), true);
shouldBe(/(?<=\1(\u{10400}))b/iu.test("\u{10429}\u{10400}b"), false);
shouldBe("\u{1f600}\u{1f600}b \u{1f600}\u{1f601}b aab".replace(/(?<=\1(.))b/gsu, "[$&|$1]"), "\u{1f600}\u{1f600}[b|\u{1f600}] \u{1f600}\u{1f601}b aa[b|a]");
shouldBe(matchAllOf(/(?<=\1(.))b/gsu, "\u{1f600}\u{1f600}b \u{1f600}\u{1f601}b aab"), [[4, "b", "\u{1f600}"], [14, "b", "a"]]);
shouldBe("\u{1f600}\u{1f600}b \u{1f600}\u{1f601}b aab".split(/(?<=\1(.))b/su), ["\u{1f600}\u{1f600}", "\u{1f600}", " \u{1f600}\u{1f601}b aa", "a", ""]);
shouldBe("\u{1f600}\u{1f600}b \u{1f600}\u{1f601}b aab".search(/(?<=\1(.))b/su), 4);
shouldBe(indicesOf(/(?<=\1(.))b/dsu, "\u{1f600}\u{1f600}b"), [4, "b", "\u{1f600}", [4, 5], [2, 4]]);
shouldBe(indicesOf(/(?<=\1(.+))b/dsu, "\u{1f600}a\u{1f600}ab"), [6, "b", "\u{1f600}a", [6, 7], [3, 6]]);
shouldBe(execAt(/(?<=\1(.))b/suy, "\u{1f600}\u{1f600}b", 4), [[4, "b", "\u{1f600}"], 5]);
shouldBe(execAt(/(?<=\1(.))b/suy, "\u{1f600}\u{1f600}b", 3), [null, 0]);
shouldBe(execAt(/(?<=\1(.))b/gsu, "\u{1f600}\u{1f600}b\u{1f601}\u{1f601}b", 5), [[9, "b", "\u{1f601}"], 10]);
shouldBe(matchOf(/(?<=\1(.{20}))x/su, "\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}x"), [80, "x", "\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}"]);
shouldBe(matchOf(/(?<=\1(.{20}))x/su, "\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f601}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}x"), null);
shouldBe(matchOf(/(?<=\1(.{20}))x/su, "\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f600}\u{1f601}x"), null);
shouldBe(matchOf(/(?<=x\1*(\u{1F600}a))c/u, "x\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}ac"), [91, "c", "\u{1f600}a"]);
shouldBe(matchOf(/(?<=x\1*(\u{1F600}a))c/u, "y\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}ac"), null);
shouldBe(matchOf(/(\u{1F600}a)c(?<=x\1+c)/u, "x\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}ac"), [88, "\u{1f600}ac", "\u{1f600}a"]);
shouldBe(matchOf(/(\u{1F600}a)c(?<=x\1+c)/u, "y\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}ac"), null);
shouldBe(matchOf(/(\u{1F600}a)c(?<=x\1{29}c)/u, "x\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}ac"), null);
shouldBe(matchOf(/(\u{1F600}a)c(?<=x\1{29}c)/u, "x\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}a\u{1f600}ac"), null);
