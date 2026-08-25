function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected " + expected);
}

function stringify(match) {
    if (match === null)
        return "null";
    return "[" + [match.index, ...Array.from(match, (x) => x === undefined ? "undefined" : JSON.stringify(x))].join(",") + "]";
}

function stringifyIndices(match) {
    if (match === null)
        return "null";
    return JSON.stringify(Array.from(match.indices, (x) => x === undefined ? null : x));
}

function shouldMatchAt(re, lastIndex, string, expectedIndex, expectedLastIndex) {
    re.lastIndex = lastIndex;
    let match = re.exec(string);
    shouldBe(match === null ? null : match.index, expectedIndex);
    shouldBe(re.lastIndex, expectedLastIndex);
}

shouldBe(stringify(/(?<=(?=\u{1F600})\u{1F600})b/u.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=\u{1F600})\u{1F600})b/u.exec("\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?=\u{1F600})\u{1F600})b/u.exec("ab")), "null");
shouldBe(stringify(/(?<=(?=\u{1F600})\u{1F600})b/v.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=^\u{1F600})\u{1F600})b/u.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=^\u{1F600})\u{1F600})b/u.exec("a\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=^\u{1F600})\u{1F600})b/u.exec("\ud83d\ude01\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=^)\u{1F600})b/u.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=^)\u{1F600})b/u.exec("a\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=\u{1F600}(?=^))b/u.exec("\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=\u{1F601}(?=^)\u{1F600})b/mu.exec("\ud83d\ude01\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=\n(?=^\u{1F600})\u{1F600})b/mu.exec("\n\ud83d\ude00b")), "[3,\"b\"]");
shouldBe(stringify(/(?<=\n(?=^\u{1F600})\u{1F600})b/u.exec("\n\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=.)\u{1F600})b/u.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=.)\u{1F600})b/u.exec("\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?=.b)\u{1F600})b/u.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=.b).)b/u.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=.b).)b/u.exec("ab")), "[1,\"b\"]");
shouldBe(stringify(/(?<=(?=..)..)b/u.exec("\ud83d\ude00\ud83d\ude01b")), "[4,\"b\"]");
shouldBe(stringify(/(?<=(?=..)..)b/u.exec("\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=..)..)b/u.exec("a\ud83d\ude00b")), "[3,\"b\"]");
shouldBe(stringify(/(?<=(?=(.))\u{1F600})b/u.exec("\ud83d\ude00b")), "[2,\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(?=(.))\u{1F600})b/u.exec("\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?=(.)(.)).\u{1F600})b/u.exec("a\ud83d\ude00b")), "[3,\"b\",\"a\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(?=(.)(.)).\u{1F600})b/u.exec("\ud83d\ude01\ud83d\ude00b")), "[4,\"b\",\"\ud83d\ude01\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(?=([\u{1F600}-\u{1F64F}]))\u{1F600})b/u.exec("\ud83d\ude00b")), "[2,\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(?=([\u{1F600}-\u{1F64F}]))\u{1F600})b/u.exec("\ud83d\udf00b")), "null");
shouldBe(stringify(/(?<=(?=[^\u{1F600}])\u{1F601})b/u.exec("\ud83d\ude01b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=[^\u{1F600}])\u{1F601})b/u.exec("\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=[^\u{1F600}]).)b/u.exec("\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=[^\u{1F600}]).)b/u.exec("ab")), "[1,\"b\"]");
shouldBe(stringify(/(?<=(?!\u{1F600})\u{1F601})b/u.exec("\ud83d\ude01b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?!\u{1F600})\u{1F601})b/u.exec("\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?!\u{1F600}).)b/u.exec("\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?!\u{1F600}).)b/u.exec("\ud83d\ude01b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?!\u{1F600}).)b/u.exec("ab")), "[1,\"b\"]");
shouldBe(stringify(/(?<=(?=\u{1F600}$)\u{1F600})/u.exec("a\ud83d\ude00")), "[3,\"\"]");
shouldBe(stringify(/(?<=(?=\u{1F600}$)\u{1F600})/u.exec("a\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=\u{1F600}(?=$))/u.exec("a\ud83d\ude00")), "[3,\"\"]");
shouldBe(stringify(/(?<=\u{1F600}(?=$))/u.exec("a\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=\u{1F600}(?=\u{1F601}))\u{1F601}/u.exec("\ud83d\ude00\ud83d\ude01")), "[2,\"\ud83d\ude01\"]");
shouldBe(stringify(/(?<=\u{1F600}(?=\u{1F601}))\u{1F601}/u.exec("\ud83d\ude01\ud83d\ude01")), "null");
shouldBe(stringify(/(?<=(?=\b)a)b/u.exec("ab")), "[1,\"b\"]");
shouldBe(stringify(/(?<=(?=\b)a)b/u.exec("\ud83d\ude00ab")), "[3,\"b\"]");
shouldBe(stringify(/(?<=(?=\B)a)b/u.exec("\ud83d\ude00ab")), "null");
shouldBe(stringify(/(?<=(?=\B)a)b/u.exec("xab")), "[2,\"b\"]");
shouldBe(stringify(/(?<=\u{1F600}(?=\b)a)b/u.exec("\ud83d\ude00ab")), "[3,\"b\"]");
shouldBe(stringify(/(?<=\u{1F600}(?=\B)a)b/u.exec("\ud83d\ude00ab")), "null");
shouldBe(stringify(/(?<=(?=\u{1F600}+b)\u{1F600}+)b/u.exec("\ud83d\ude00\ud83d\ude00b")), "[4,\"b\"]");
shouldBe(stringify(/(?<=(?=\u{1F600}+b)\u{1F600}+)b/u.exec("\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?=\u{1F600}*b).*)b/u.exec("\ud83d\ude00\ud83d\ude00b")), "[4,\"b\"]");
shouldBe(stringify(/(?<=(?=(?:\u{1F600}\u{1F601})+b).+)b/u.exec("\ud83d\ude00\ud83d\ude01\ud83d\ude00\ud83d\ude01b")), "[8,\"b\"]");
shouldBe(stringify(/(?<=(?=(?:\u{1F600}\u{1F601})+b).+)b/u.exec("\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=(\u{1F600}|\u{1F601})\1)..)b/u.exec("\ud83d\ude00\ud83d\ude00b")), "[4,\"b\",\"\ud83d\ude00\"]");
shouldBe(stringify(/(?<=(?=(\u{1F600}|\u{1F601})\1)..)b/u.exec("\ud83d\ude00\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?=(?<=\u{1F600})\u{1F601})\u{1F601})b/u.exec("\ud83d\ude00\ud83d\ude01b")), "[4,\"b\"]");
shouldBe(stringify(/(?<=(?=(?<=\u{1F600})\u{1F601})\u{1F601})b/u.exec("\ud83d\ude01\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?=(?<=^\u{1F600})\u{1F601})\u{1F601})b/u.exec("\ud83d\ude00\ud83d\ude01b")), "[4,\"b\"]");
shouldBe(stringify(/(?<=(?=(?<=^\u{1F600})\u{1F601})\u{1F601})b/u.exec("a\ud83d\ude00\ud83d\ude01b")), "null");
shouldBe(stringify(/(?<=(?=\u{1F600}(?=\u{1F601}))\u{1F600}\u{1F601})b/u.exec("\ud83d\ude00\ud83d\ude01b")), "[4,\"b\"]");
shouldBe(stringify(/(?<=(?=\u{1F600}(?=\u{1F601}))\u{1F600}\u{1F601})b/u.exec("\ud83d\ude00\ud83d\ude00b")), "null");
shouldBe(stringify(/(?<=(?=\u{1F600}|a)\w)b/u.exec("ab")), "[1,\"b\"]");
shouldBe(stringify(/(?<=(?=\u{1F600}|a)\w)b/u.exec("cb")), "null");
shouldBe(stringify(/(?<=(?=\u{1F600}|a).)b/u.exec("\ud83d\ude00b")), "[2,\"b\"]");
shouldBe(stringify(/(?<=(?=k)k)x/iu.exec("\u212ax")), "[1,\"x\"]");
shouldBe(stringify(/(?<=(?=\u212a)k)x/iu.exec("kx")), "[1,\"x\"]");
shouldBe(stringify(/(?<=(?=\u{10400})\u{10428})x/iu.exec("\ud801\udc28x")), "[2,\"x\"]");
shouldBe(stringify(/(?<=(?=\u{10400})\u{10428})x/u.exec("\ud801\udc28x")), "null");
shouldBe(stringify(/(?<=(?=^\u{10400})\u{10428})x/iu.exec("\ud801\udc28x")), "[2,\"x\"]");
shouldBe(stringify(/(?<=(?=^\u{10400})\u{10428})x/iu.exec("a\ud801\udc28x")), "null");
shouldBe(stringify(/(?<=(?=^)a)b/u.exec("ab")), "[1,\"b\"]");
shouldBe(stringify(/(?<=(?=^)a)b/u.exec("\ud83d\ude00ab")), "null");
shouldBe(stringify(/(?<=\b(?!un)\w+)able/u.exec("doable")), "[2,\"able\"]");
shouldBe(stringify(/(?<=\b(?!un)\w+)able/u.exec("unable")), "null");
shouldBe(stringify(/(?<=\b(?!\u{1F600})\w+)able/u.exec("doable")), "[2,\"able\"]");
shouldBe(stringify(/(?<=\b(?!\u{1F600}).+)able/u.exec("\ud83d\ude00able")), "null");
shouldBe(stringify(/(?<=\b(?!\u{1F600}).+)able/u.exec("\ud83d\ude01able")), "null");

shouldMatchAt(/(?<=(?=^)\u{1F600})b/gu, 0, "\ud83d\ude00b\ud83d\ude00b", 2, 3);
shouldMatchAt(/(?<=(?=^)\u{1F600})b/gu, 1, "\ud83d\ude00b\ud83d\ude00b", 2, 3);
shouldMatchAt(/(?<=(?=^)\u{1F600})b/gu, 2, "\ud83d\ude00b\ud83d\ude00b", 2, 3);
shouldMatchAt(/(?<=(?=^)\u{1F600})b/gu, 3, "\ud83d\ude00b\ud83d\ude00b", null, 0);
shouldMatchAt(/(?<=(?=^)\u{1F600})b/yu, 0, "\ud83d\ude00b\ud83d\ude00b", null, 0);
shouldMatchAt(/(?<=(?=^)\u{1F600})b/yu, 2, "\ud83d\ude00b\ud83d\ude00b", 2, 3);
shouldMatchAt(/(?<=(?=^)\u{1F600})b/yu, 3, "\ud83d\ude00b\ud83d\ude00b", null, 0);
shouldMatchAt(/(?<=(?=^)\u{1F600})b/yu, 5, "\ud83d\ude00b\ud83d\ude00b", null, 0);
shouldMatchAt(/(?<=(?=\b)\u{1F600}a)b/gu, 0, "\ud83d\ude00ab \ud83d\ude00ab", null, 0);
shouldMatchAt(/(?<=(?=\b)\u{1F600}a)b/gu, 1, "\ud83d\ude00ab \ud83d\ude00ab", null, 0);
shouldMatchAt(/(?<=(?=\b)\u{1F600}a)b/gu, 2, "\ud83d\ude00ab \ud83d\ude00ab", null, 0);
shouldMatchAt(/(?<=(?=\b)\u{1F600}a)b/gu, 3, "\ud83d\ude00ab \ud83d\ude00ab", null, 0);
shouldMatchAt(/(?<=(?=\b)\u{1F600}a)b/gu, 4, "\ud83d\ude00ab \ud83d\ude00ab", null, 0);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gu, 0, "\ud83d\ude00\ud83d\ude00", 4, 4);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gu, 1, "\ud83d\ude00\ud83d\ude00", 4, 4);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gu, 2, "\ud83d\ude00\ud83d\ude00", 4, 4);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gu, 3, "\ud83d\ude00\ud83d\ude00", 4, 4);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gu, 4, "\ud83d\ude00\ud83d\ude00", 4, 4);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gmu, 0, "\ud83d\ude00\n\ud83d\ude00", 2, 2);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gmu, 1, "\ud83d\ude00\n\ud83d\ude00", 2, 2);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gmu, 2, "\ud83d\ude00\n\ud83d\ude00", 2, 2);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gmu, 3, "\ud83d\ude00\n\ud83d\ude00", 5, 5);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gmu, 4, "\ud83d\ude00\n\ud83d\ude00", 5, 5);
shouldMatchAt(/(?<=(?=\u{1F600}$)\u{1F600})/gmu, 5, "\ud83d\ude00\n\ud83d\ude00", 5, 5);
shouldMatchAt(/(?<=(?=(.))\u{1F600})b/gu, 0, "\ud83d\ude00b\ud83d\ude00b", 2, 3);
shouldMatchAt(/(?<=(?=(.))\u{1F600})b/gu, 1, "\ud83d\ude00b\ud83d\ude00b", 2, 3);
shouldMatchAt(/(?<=(?=(.))\u{1F600})b/gu, 2, "\ud83d\ude00b\ud83d\ude00b", 2, 3);
shouldMatchAt(/(?<=(?=(.))\u{1F600})b/gu, 3, "\ud83d\ude00b\ud83d\ude00b", 5, 6);

shouldBe("\ud83d\ude00b\ud83d\ude00b".replace(/(?<=(?=^)\u{1F600})b/gu, "-"), "\ud83d\ude00-\ud83d\ude00b");
shouldBe("\ud83d\ude00b\ud83d\ude00b".replace(/(?<=(?=(.))\u{1F600})b/gu, "[$1]"), "\ud83d\ude00[\ud83d\ude00]\ud83d\ude00[\ud83d\ude00]");
shouldBe("\ud83d\ude00b\ud83d\ude00b".replace(/(?<=(?=(.)|(.))\u{1F600})b/gu, "[$1$2]"), "\ud83d\ude00[\ud83d\ude00]\ud83d\ude00[\ud83d\ude00]");
shouldBe("\ud83d\ude00able \ud83d\ude01able".replace(/(?<=\b(?!\u{1F600}).+)able/gu, "ABLE"), "\ud83d\ude00able \ud83d\ude01ABLE");

shouldBe(stringifyIndices(/(?<=(?=(.))\u{1F600})b/du.exec("\ud83d\ude00b")), "[[2,3],[0,2]]");
shouldBe(stringifyIndices(/(?<=(?=(.)(.)).\u{1F600})b/du.exec("a\ud83d\ude00b")), "[[3,4],[0,1],[1,3]]");
shouldBe(stringifyIndices(/(?<=(?=(\u{1F600}|\u{1F601})\1)..)b/du.exec("\ud83d\ude00\ud83d\ude00b")), "[[4,5],[0,2]]");
shouldBe(stringifyIndices(/(?<=(?=(?<=(\u{1F600}))\u{1F601})\u{1F601})b/du.exec("\ud83d\ude00\ud83d\ude01b")), "[[4,5],[0,2]]");
