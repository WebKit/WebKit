function shouldBe(actual, expected, msg) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/\.js$/.exec("path/app.js"), [".js"], "literal suffix");
    shouldBe(/\.js$/.exec("path/app.jsx"), null, "literal suffix mismatch");
    shouldBe(/abcdef$/.exec("def"), null, "pattern longer than subject");
    shouldBe(/(?:\.png|\.gif)$/.exec("x.gif"), [".gif"], "equal-size alternatives");
    shouldBe(/(?:d|cd)$/.exec("abcd"), ["cd"], "different-size alternatives");
    shouldBe(/(?:x|abcd)$/.exec("ax"), ["x"], "different-size alternatives, shorter one matches");
    shouldBe(/(\w)(\d)$/.exec("ab3zx9"), ["x9", "x", "9"], "captures");
    shouldBe("abc".search(/$/), 3, "bare end assertion");
    shouldBe(/(?:)$/.exec("ab").index, 2, "empty group then end");
    shouldBe([/^abc$/.exec("abc"), /^abc$/.exec("xabc"), /^abc$/.exec("abcabc")], [["abc"], null, null], "BOL and EOL");
    shouldBe(["a\nbc\nd".search(/bc$/m), /bc$/m.exec("a\nbc\nd")], [2, ["bc"]], "multiline is untouched");
    shouldBe(/(a)\1$/.exec("xaa"), ["aa", "a"], "backreference (variable size) is untouched");
    shouldBe(/[0-9a-f]{4}$/.exec("zzz9f0a"), ["9f0a"], "counted class");
    shouldBe(/[0-9a-f]{4}$/.exec("zzz9f0az"), null, "counted class mismatch");
    shouldBe([/\u{1F600}$/u.exec("hi\u{1F600}"), /.$/u.exec("a\u{1F600}")], [["\u{1F600}"], ["\u{1F600}"]], "unicode fixed size counts code units");
    shouldBe([/(?<=x)ab$/.exec("xab"), /(?<=x)ab$/.exec("yab")], [["ab"], null], "lookbehind before the end");
    shouldBe([/a(?=b$)b$/.exec("zab"), /(?!x)y$/.exec("xy")], [["ab"], ["y"]], "lookahead");
    shouldBe([/\bfoo$/.exec("a foo"), /\Bfoo$/.exec("afoo"), /\bfoo$/.exec("afoo")], [["foo"], ["foo"], null], "word boundary before literal");
    shouldBe([/\.JS$/i.exec("a.js"), /ÅB$/iu.exec("xåb")], [[".js"], ["åb"]], "ignoreCase");
    shouldBe("a.b.c".replace(/\.c$/, "!"), "a.b!", "replace");
    shouldBe(("x".repeat(300) + ".ts").search(/\.ts$/), 300, "search");
    shouldBe([..."foo.ts".matchAll(/ts$/g)].map(m => m.index), [4], "matchAll");
    shouldBe(/(t)s$/d.exec("a.ts").indices, [[2, 4], [2, 3]], "hasIndices");
    shouldBe([/$/.exec("")[0], /a$/.exec("")], ["", null], "empty subject");
    shouldBe([/\uDE00$/u.exec("\u{1F600}"), /\uDE00$/.exec("\u{1F600}")], [null, ["\uDE00"]], "lone surrogate at the end");
    shouldBe("x".repeat(1000).replace(/xxx$/, "!"), "x".repeat(997) + "!", "long subject replace");

    let re = /js$/g;
    let s = "a.js";
    let all = [];
    let m;
    while ((m = re.exec(s))) {
        all.push([m.index, re.lastIndex]);
        if (all.length > 3)
            break;
    }
    shouldBe(all, [[2, 4]], "global iteration");

    re = /js$/g;
    re.lastIndex = 100;
    shouldBe([re.exec("a.js"), re.lastIndex], [null, 0], "lastIndex beyond the subject");

    re = /js$/g;
    re.lastIndex = 3;
    shouldBe(re.exec("a.js"), null, "lastIndex past the only possible start");

    re = /js$/y;
    re.lastIndex = 0;
    shouldBe(re.exec("index.js"), null, "sticky does not search");
    re.lastIndex = 6;
    m = re.exec("index.js");
    shouldBe(m && m.index, 6, "sticky matches only at lastIndex");
}
